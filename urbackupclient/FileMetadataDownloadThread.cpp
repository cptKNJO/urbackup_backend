/*************************************************************************
*    UrBackup - Client/Server backup system
*    Copyright (C) 2011-2016 Martin Raiber
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU Affero General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include "FileMetadataDownloadThread.h"
#include "../urbackupcommon/file_metadata.h"
#include "../urbackupcommon/os_functions.h"
#include "../common/data.h"
#include "../common/adler32.h"
#include <assert.h>
#include <limits.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>
#ifndef __FreeBSD__
#include <sys/xattr.h>
#endif
#endif
#include <memory>

#ifdef __APPLE__
#define llistxattr(path, list, size) listxattr(path, list, size, XATTR_NOFOLLOW)
#define lremovexattr(path, name) removexattr(path, name, XATTR_NOFOLLOW)
#define lsetxattr(path, name, value, size, flags) setxattr(path, name, value, size, 0, XATTR_NOFOLLOW|flags)
#elif __FreeBSD__
#define O_SYMLINK 0
#define stat64 stat
#ifndef UF_HIDDEN
#define UF_HIDDEN 0
#endif
#ifndef UF_ARCHIVE
#define UF_ARCHIVE 0
#endif
#ifndef UF_OFFLINE
#define UF_OFFLINE 0
#endif
#ifndef UF_READONLY
#define UF_READONLY 0
#endif
#ifndef UF_REPARSE
#define UF_REPARSE 0
#endif
#ifndef UF_SPARSE
#define UF_SPARSE 0
#endif
#ifndef UF_SYSTEM
#define UF_SYSTEM 0
#endif
//TODO implement using extattr
namespace
{
	ssize_t llistxattr(const char *path, char *list, size_t size)
	{
		errno = EOPNOTSUPP;
		return -1;
	}

	int lremovexattr(const char *path, const char *name)
	{
		errno = EOPNOTSUPP;
		return -1;
	}

	int lsetxattr(const char *path, const char *name,
		const void *value, size_t size, int flags)
	{
		errno = EOPNOTSUPP;
		return -1;
	}
}
#endif

namespace client {

const _u32 ID_METADATA_OS_WIN = 1<<0;
const _u32 ID_METADATA_OS_UNIX = 1<<2;

const _u32 ID_METADATA_NOP = 0;
const _u32 ID_METADATA_V1 = 1<<3;
#ifdef _WIN32
const _u32 ID_METADATA_OS = ID_METADATA_OS_WIN;
#else
const _u32 ID_METADATA_OS = ID_METADATA_OS_UNIX;
#endif

FileMetadataDownloadThread::FileMetadataDownloadThread(RestoreFiles& restore, FileClient& fc, const std::string& client_token)
	: restore(restore), fc(fc), client_token(client_token), has_error(false), error_logging(true)
{

}

void FileMetadataDownloadThread::operator()()
{
	std::unique_ptr<IFsFile> tmp_f(Server->openTemporaryFile());

	if(tmp_f.get()==nullptr)
	{
		restore.log("Error creating temporary file for metadata", LL_ERROR);
	}
	
	std::string remote_fn = "SCRIPT|urbackup/FILE_METADATA|"+client_token;

	_u32 rc = fc.GetFile(remote_fn, tmp_f.get(), true, false, 0, true, 0);

	if(rc!=ERR_SUCCESS)
	{
		if (error_logging)
		{
			restore.log("Error getting file metadata. Errorcode: " + FileClient::getErrorString(rc) + " (" + convert(rc) + ")", LL_ERROR);
		}
		has_error=true;
	}
	else
	{
		has_error=false;

		fc.FinishScript(remote_fn);
	}

	metadata_tmp_fn = tmp_f->getFilename();
}

bool FileMetadataDownloadThread::applyMetadata(const str_map& path_mapping)
{
	buffer.resize(32768);
	std::unique_ptr<IFile> metadata_f(Server->openFile(metadata_tmp_fn, MODE_READ_SEQUENTIAL));

	if(metadata_f.get()==nullptr)
	{
		restore.log("Error opening metadata file at \""+ metadata_tmp_fn+"\". Cannot save file metadata. "+os_last_error_str(), LL_ERROR);
		return false;
	}

	restore.log("Applying file metadata...", LL_INFO);

	do 
	{
		char ch;
		if(metadata_f->Read(reinterpret_cast<char*>(&ch), sizeof(ch))!=sizeof(ch))
		{
			return true;
		}

		if(ch==ID_METADATA_NOP)
		{
			continue;
		}
		
		if(ch & ID_METADATA_V1)
		{
			unsigned int curr_fn_size =0;
			if(metadata_f->Read(reinterpret_cast<char*>(&curr_fn_size), sizeof(curr_fn_size))!=sizeof(curr_fn_size))
			{
				restore.log("Error saving metadata. Filename size could not be read.", LL_ERROR);
				return false;
			}

			unsigned int path_checksum = urb_adler32(urb_adler32(0, nullptr, 0), reinterpret_cast<char*>(&curr_fn_size), sizeof(curr_fn_size));

			std::string curr_fn;

			if(curr_fn_size>0)
			{			
				curr_fn.resize(little_endian(curr_fn_size));

				if(metadata_f->Read(&curr_fn[0], static_cast<_u32>(curr_fn.size()))!=curr_fn.size())
				{
					restore.log("Error saving metadata. Filename could not be read.", LL_ERROR);
					return false;
				}

				path_checksum = urb_adler32(path_checksum, &curr_fn[0], static_cast<_u32>(curr_fn.size()));
			}

			if(curr_fn.empty())
			{
				restore.log("Error saving metadata. Filename is empty.", LL_ERROR);
				return false;
			}

			unsigned int read_path_checksum =0;
			if(metadata_f->Read(reinterpret_cast<char*>(&read_path_checksum), sizeof(read_path_checksum))!=sizeof(read_path_checksum))
			{
				restore.log( "Error saving metadata. Path checksum could not be read.", LL_ERROR);
				return false;
			}

			if(little_endian(read_path_checksum)!=path_checksum)
			{
				restore.log("Error saving metadata. Path checksum wrong.", LL_ERROR);
				return false;
			}

			restore.log("Applying metadata of file \"" + curr_fn + "\"", LL_DEBUG);

			bool is_dir = (curr_fn[0]=='d' || curr_fn[0]=='l');

#ifdef _WIN32
			std::string os_path;
#else
            std::string os_path = os_file_sep();
#endif
			std::vector<std::string> fs_toks;
			Tokenize(curr_fn.substr(1), fs_toks, "/");

			for(size_t i=0;i<fs_toks.size();++i)
			{
				if(fs_toks[i]!="." && fs_toks[i]!=".." && !fs_toks[i].empty())
				{
					if (!os_path.empty()
						&& os_path != os_file_sep())
					{
						os_path += os_file_sep();
					}

					os_path += (fs_toks[i]);				
				}
			}

			str_map::const_iterator it_pm = path_mapping.find(os_path);
			if (it_pm != path_mapping.end())
			{
				os_path = it_pm->second;
			}

			bool ok=false;
			if(ch & ID_METADATA_OS)
			{
				ok = applyOsMetadata(metadata_f.get(), os_path);
			}
			else
			{
				restore.log("Wrong metadata. This metadata is not for this operating system! (id=" + convert(ch)+")", LL_ERROR);
			}

			if(!ok)
			{
				restore.log("Error saving metadata. Could not save OS specific metadata to \"" + os_path + "\"", LL_ERROR);
				return false;
			}			
		}

	} while (true);

	Server->Log("Loop exit in " + std::string(__FUNCTION__), LL_ERROR);
	assert(false);
	return true;
}

#ifdef _WIN32
namespace
{
	struct WIN32_STREAM_ID_INT
	{
		_u32 dwStreamId;
		_u32 dwStreamAttributes;
		int64 Size;
		_u32 dwStreamNameSize;
	};

	const size_t metadata_id_size = 4+4+8+4;
	const int64 win32_meta_magic = little_endian(0x320FAB3D119DCB4A);

	class HandleScope
	{
	public:
		HandleScope(HANDLE hfile):
		  hfile(hfile) {}

		  ~HandleScope(){
			  if (hfile != INVALID_HANDLE_VALUE)
			  {
				  CloseHandle(hfile);
			  }
		  }
	private:
		HANDLE hfile;
	};
}

bool FileMetadataDownloadThread::applyOsMetadata( IFile* metadata_f, const std::string& output_fn)
{
	int64 win32_magic_and_size[2];
	unsigned int data_checksum = urb_adler32(0, NULL, 0);

	if(metadata_f->Read(reinterpret_cast<char*>(win32_magic_and_size), sizeof(win32_magic_and_size))!=sizeof(win32_magic_and_size))
	{
		restore.log("Error reading  \"" + metadata_f->getFilename() + "\"", LL_ERROR);
		has_error=true;
		return false;
	}

	data_checksum = urb_adler32(data_checksum, reinterpret_cast<char*>(win32_magic_and_size), sizeof(win32_magic_and_size));

	if(win32_magic_and_size[1]!=win32_meta_magic)
	{
		restore.log("Win32 metadata magic wrong in  \"" + metadata_f->getFilename() + "\"", LL_ERROR);
		has_error=true;
		return false;
	}

	_u32 stat_data_size;
	if(metadata_f->Read(reinterpret_cast<char*>(&stat_data_size), sizeof(_u32))!=sizeof(_u32))
	{
		restore.log("Error reading stat data size from \"" + metadata_f->getFilename() + "\"", LL_ERROR);
		return false;
	}

	data_checksum = urb_adler32(data_checksum, reinterpret_cast<char*>(&stat_data_size), sizeof(_u32));

	stat_data_size = little_endian(stat_data_size);

	if(stat_data_size<1)
	{
		restore.log("Stat data size from \"" + metadata_f->getFilename() + "\" is zero", LL_ERROR);
		return false;
	}

	char version=-1;    

	std::vector<char> stat_data;
	stat_data.resize(stat_data_size);
	if(metadata_f->Read(stat_data.data(), static_cast<_u32>(stat_data.size()))!=stat_data.size())
	{
		restore.log("Error reading windows metadata from \"" + metadata_f->getFilename() + "\"", LL_ERROR);
		return false;
	}

	data_checksum = urb_adler32(data_checksum, stat_data.data(), static_cast<_u32>(stat_data.size()));

	CRData read_stat(stat_data.data(), stat_data.size());

	if(!read_stat.getChar(&version) || version!=1)
	{
		restore.log("Unknown windows metadata version "+convert((int)version)+" in \"" + metadata_f->getFilename() + "\"", LL_ERROR);
		return false;
	}

	FILE_BASIC_INFO basic_info;

	if (!read_stat.getUInt(reinterpret_cast<unsigned int*>(&basic_info.FileAttributes)))
	{
		restore.log("Error getting FileAttributes of file \"" + output_fn + "\".", LL_ERROR);
		has_error = true;
	}

	if ((basic_info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		&& (basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		&& !(os_get_file_type(os_file_prefix(output_fn)) & EFileType_Directory))
	{
		Server->deleteFile(os_file_prefix(output_fn));
		os_create_dir(os_file_prefix(output_fn));
	}
	else if((basic_info.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		&& !(basic_info.FileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		&& (os_get_file_type(os_file_prefix(output_fn)) & EFileType_Directory) )
	{
		os_remove_dir(os_file_prefix(output_fn));
		std::unique_ptr<IFile> touch_file(Server->openFile(os_file_prefix(output_fn), MODE_WRITE));
	}

	HANDLE hFile = CreateFileW(Server->ConvertToWchar(os_file_prefix(output_fn)).c_str(), GENERIC_WRITE | ACCESS_SYSTEM_SECURITY | WRITE_OWNER | WRITE_DAC, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN | FILE_FLAG_OPEN_REPARSE_POINT, NULL);

	if (hFile == INVALID_HANDLE_VALUE
		&& output_fn.size()>3)
	{
		restore.log("Cannot open handle to restore file metadata of file \"" + output_fn + "\". " + os_last_error_str(), LL_ERROR);
		return false;
	}

	HandleScope handle_scope(hFile);

	bool has_error=false;
	void* context = NULL;

	while(true) 
	{
		char cont = 0;
		if(metadata_f->Read(&cont, sizeof(cont))!=sizeof(cont))
		{
			restore.log("Error reading  \"" + metadata_f->getFilename() + "\"", LL_ERROR);
			has_error=true;
			break;
		}

		data_checksum = urb_adler32(data_checksum, &cont, 1);

		if(cont==0)
		{
			break;
		}

		std::vector<char> stream_id;
		stream_id.resize(metadata_id_size);

		if(metadata_f->Read(stream_id.data(), metadata_id_size)!=metadata_id_size)
		{
			restore.log("Error reading  \"" + metadata_f->getFilename() + "\"", LL_ERROR);
			has_error=true;
			break;
		}

		data_checksum = urb_adler32(data_checksum, stream_id.data(), metadata_id_size);

		WIN32_STREAM_ID_INT* curr_stream_id =
			reinterpret_cast<WIN32_STREAM_ID_INT*>(stream_id.data());

		if(curr_stream_id->dwStreamNameSize>0)
		{
			stream_id.resize(stream_id.size()+curr_stream_id->dwStreamNameSize);

			curr_stream_id =
				reinterpret_cast<WIN32_STREAM_ID_INT*>(stream_id.data());

			if(metadata_f->Read(stream_id.data() + metadata_id_size, static_cast<_u32>(curr_stream_id->dwStreamNameSize))!=curr_stream_id->dwStreamNameSize)
			{
				restore.log("Error reading  \"" + metadata_f->getFilename() + "\" -2", LL_ERROR);
				has_error=true;
				break;
			}

			data_checksum = urb_adler32(data_checksum, stream_id.data() + metadata_id_size, static_cast<_u32>(curr_stream_id->dwStreamNameSize));
		}	

		if (hFile != INVALID_HANDLE_VALUE)
		{
			DWORD written = 0;
			BOOL b = BackupWrite(hFile, reinterpret_cast<LPBYTE>(stream_id.data()), static_cast<DWORD>(stream_id.size()), &written, FALSE, TRUE, &context);

			if (!b || written != stream_id.size())
			{
				restore.log("Error writting metadata to file \"" + output_fn + "\". "+os_last_error_str(), LL_ERROR);
				has_error = true;
				break;
			}
		}

		int64 curr_pos=0;

		while(curr_pos<curr_stream_id->Size)
		{
			_u32 toread = static_cast<_u32>((std::min)(static_cast<int64>(buffer.size()), curr_stream_id->Size-curr_pos));

			if(metadata_f->Read(buffer.data(), toread)!=toread)
			{
				restore.log("Error reading  \"" + metadata_f->getFilename() + "\" -3", LL_ERROR);
				has_error=true;
				break;
			}

			data_checksum = urb_adler32(data_checksum, buffer.data(), toread);

			if (hFile != INVALID_HANDLE_VALUE)
			{
				DWORD written = 0;
				BOOL b = BackupWrite(hFile, reinterpret_cast<LPBYTE>(buffer.data()), toread, &written, FALSE, TRUE, &context);

				if (!b || written != toread)
				{
					restore.log("Error writting metadata to file \"" + output_fn + "\". "+os_last_error_str(), LL_ERROR);
					has_error = true;
					break;
				}
			}

			curr_pos+=toread;
		}
	}

	if(context!=NULL
		&& hFile != INVALID_HANDLE_VALUE)
	{
		DWORD written;
		BackupWrite(hFile, NULL, 0, &written, TRUE, TRUE, &context);
	}

	unsigned int read_data_checksum =0;
	if(metadata_f->Read(reinterpret_cast<char*>(&read_data_checksum), sizeof(read_data_checksum))!=sizeof(read_data_checksum))
	{
		restore.log( "Error saving metadata. Data checksum could not be read.", LL_ERROR);
		return false;
	}

	if(little_endian(read_data_checksum)!=data_checksum)
	{
		restore.log("Error saving metadata. Data checksum wrong.", LL_ERROR);
		return false;
	}

	if(!read_stat.getVarInt(&basic_info.CreationTime.QuadPart))
	{
		restore.log("Error getting CreationTime of file \""+output_fn+"\".", LL_ERROR);
		has_error=true;
	}

	if(!read_stat.getVarInt(&basic_info.LastAccessTime.QuadPart))
	{
		restore.log("Error getting LastAccessTime of file \""+output_fn+"\".", LL_ERROR);
		has_error=true;
	}

	if(!read_stat.getVarInt(&basic_info.LastWriteTime.QuadPart))
	{
		restore.log("Error getting LastWriteTime of file \""+output_fn+"\".", LL_ERROR);
		has_error=true;
	}

	if(!read_stat.getVarInt(&basic_info.ChangeTime.QuadPart))
	{
		restore.log("Error getting LastWriteTime of file \""+output_fn+"\".", LL_ERROR);
		has_error=true;
	}

	if (hFile != INVALID_HANDLE_VALUE)
	{
		if (SetFileInformationByHandle(hFile, FileBasicInfo, &basic_info, sizeof(basic_info)) != TRUE)
		{
			restore.log("Error setting file attributes of file \"" + output_fn + "\".", LL_ERROR);
			has_error = true;
		}
	}

	return !has_error;
}

#else //_WIN32

namespace
{
    const int64 unix_meta_magic =  little_endian(0xFE4378A3467647F0ULL);

	void unserialize_stat_buf(CRData& data, struct stat64& statbuf, std::string& symlink_target)
	{
#define SET_STAT_MEM(x)  {int64 tmp; bool b = data.getVarInt(&tmp); assert(b); statbuf.x = tmp;}
#define SET_STAT_MEM32(x)  {_u32 tmp; bool b = data.getUInt(&tmp); assert(b); statbuf.x = tmp;}

		SET_STAT_MEM(st_dev);
		SET_STAT_MEM(st_mode);
		SET_STAT_MEM(st_uid);
		SET_STAT_MEM(st_gid);
		SET_STAT_MEM(st_rdev);
#if defined(__APPLE__) || defined(__FreeBSD__)
		SET_STAT_MEM(st_atimespec.tv_sec);
		SET_STAT_MEM32(st_atimespec.tv_nsec);
		SET_STAT_MEM(st_mtimespec.tv_sec);
		SET_STAT_MEM32(st_mtimespec.tv_nsec);
		SET_STAT_MEM(st_ctimespec.tv_sec);
		SET_STAT_MEM32(st_ctimespec.tv_nsec);
		SET_STAT_MEM(st_flags);
#else
		SET_STAT_MEM(st_atime);
		SET_STAT_MEM32(st_atim.tv_nsec);
		SET_STAT_MEM(st_mtime);
		SET_STAT_MEM32(st_mtim.tv_nsec);
		SET_STAT_MEM(st_ctime);
		SET_STAT_MEM32(st_ctim.tv_nsec);
		{
			int64 dummy;
			bool b = data.getVarInt(&dummy);
			assert(b);
		}
#endif
		bool b = data.getStr(&symlink_target);
		assert(b);

#undef SET_STAT_MEM
#undef SET_STAT_MEM32
    }

    bool restore_stat_buf(RestoreFiles& restore, struct stat64& statbuf, const std::string& fn, const std::string& symlink_target)
    {
#ifndef __linux__

#if defined(__APPLE__)
		int64 chflags_mask = UF_NODUMP | UF_IMMUTABLE | UF_APPEND | UF_OPAQUE | UF_HIDDEN | SF_ARCHIVED | SF_IMMUTABLE | SF_APPEND;
#else
		int64 chflags_mask = SF_APPEND | SF_ARCHIVED | SF_IMMUTABLE | SF_NOUNLINK | UF_APPEND | UF_ARCHIVE | UF_HIDDEN | UF_IMMUTABLE | UF_NODUMP | UF_NOUNLINK | UF_OFFLINE | UF_OPAQUE | UF_READONLY | UF_REPARSE | UF_SPARSE | UF_SYSTEM;
#endif

#endif //__linux__

        bool ret=true;
		bool set_owner = true;
        if(S_ISLNK(statbuf.st_mode))
        {
			unlink(fn.c_str());
			rmdir(fn.c_str());

			if (symlink(symlink_target.c_str(), fn.c_str()) != 0)
			{
				restore.log("Error creating symlink \"" + fn + "\" (to \""+symlink_target+"\") errno: " + convert(errno), LL_ERROR);
				ret = false;
			}
			else
			{
				if (lchown(fn.c_str(), statbuf.st_uid, statbuf.st_gid) != 0)
				{
					restore.log("Error setting owner of symlink \"" + fn + "\" errno: " + convert(errno), LL_ERROR);
					ret = false;
				}

				/* for utimes
				struct timeval tvs[2];
				TIMESPEC_TO_TIMEVAL(&tvs[0], &(statbuf.st_atim));
				TIMESPEC_TO_TIMEVAL(&tvs[1], &(statbuf.st_mtim));
				if(lutimes(fn.c_str(), tvs)!=0)
				*/

#if defined(__APPLE__) || defined(__FreeBSD__)
				int fd = open(fn.c_str(), O_WRONLY | O_SYMLINK | O_CLOEXEC);
				if (fd == -1)
				{
					restore.log("Error setting access and modification time of symlink \"" + fn + "\" -1 errno: " + convert(errno), LL_ERROR);
					ret = false;
				}
				else
				{
					struct timeval tv[2];
					tv[0].tv_sec = statbuf.st_atimespec.tv_sec;
					tv[0].tv_usec = statbuf.st_atimespec.tv_nsec / 1000;
					tv[1].tv_sec = statbuf.st_mtimespec.tv_sec;
					tv[1].tv_usec = statbuf.st_mtimespec.tv_nsec / 1000;
					int rc1 = futimes(fd, tv);

					if (rc1 != 0)
					{
						restore.log("Error setting access and modification time of symlink \"" + fn + "\" -2 errno: " + convert(errno), LL_ERROR);
						ret = false;
					}

					int rc2 = fchflags(fd, statbuf.st_flags & chflags_mask);

					if (rc2 != 0)
					{
						restore.log("Error setting BSD flags of symlink \"" + fn + "\" -2 errno: " + convert(errno), LL_ERROR);
						ret = false;
					}

					close(fd);
				}
#else
				timespec tss[2];
				tss[0] = statbuf.st_atim;
				tss[1] = statbuf.st_mtim;

				if (utimensat(0, fn.c_str(), tss, AT_SYMLINK_NOFOLLOW) != 0)
				{
					restore.log("Error setting access and modification time of symlink \"" + fn + "\" errno: " + convert(errno), LL_ERROR);
					ret = false;
				}
#endif
			}

            return ret;
        }
        else if(S_ISREG(statbuf.st_mode) || S_ISDIR(statbuf.st_mode) )
        {
			if (chown(fn.c_str(), statbuf.st_uid, statbuf.st_gid) != 0)
			{
				restore.log("Error setting owner of file \"" + fn + "\" errno: " + convert(errno), LL_ERROR);
				ret = false;
			}

			set_owner = false;

            if(chmod(fn.c_str(), statbuf.st_mode)!=0)
            {
                restore.log("Error changing permissions of file \""+fn+"\" errno: "+convert(errno), LL_ERROR);
                ret = false;
            }
        }
        else if(S_ISFIFO(statbuf.st_mode) )
        {
            unlink(fn.c_str());

            if(mkfifo(fn.c_str(), statbuf.st_mode)!=0)
            {
                restore.log("Error creating FIFO \""+fn+"\" errno: "+convert(errno), LL_ERROR);
                ret = false;
            }
        }
        else if(S_ISBLK(statbuf.st_mode) || S_ISCHR(statbuf.st_mode) )
        {
            unlink(fn.c_str());

            if(mknod(fn.c_str(), statbuf.st_mode, statbuf.st_rdev)!=0)
            {
                restore.log("Error creating file system node \""+fn+"\" errno: "+convert(errno), LL_ERROR);
                ret = false;
            }
        }
		else
		{
			assert(S_ISSOCK(statbuf.st_mode));
			restore.log("Not creating unix socket at \"" + fn + "\"", LL_INFO);
			return ret;
		}

        if(set_owner
			&& ret
			&& chown(fn.c_str(), statbuf.st_uid, statbuf.st_gid)!=0)
        {
            restore.log("Error setting owner of file \""+fn+"\" errno: "+convert(errno), LL_ERROR);
            ret = false;
        }

        /* for utimes
		struct timeval tvs[2];
        TIMESPEC_TO_TIMEVAL(&tvs[0], &(statbuf.st_atim));
        TIMESPEC_TO_TIMEVAL(&tvs[1], &(statbuf.st_mtim));
		if(utimes(fn.c_str(), tvs)!=0)
		*/

#if defined(__APPLE__) || defined(__FreeBSD__)
		struct timeval tv[2];
		tv[0].tv_sec = statbuf.st_atimespec.tv_sec;
		tv[0].tv_usec = statbuf.st_atimespec.tv_nsec / 1000;
		tv[1].tv_sec = statbuf.st_mtimespec.tv_sec;
		tv[1].tv_usec = statbuf.st_mtimespec.tv_nsec / 1000;
		if (utimes(fn.c_str(), tv) != 0)
		{
			restore.log("Error setting access and modification time of file \"" + fn + "\" errno: " + convert(errno), LL_ERROR);
			ret = false;
		}

		if (chflags(fn.c_str(), statbuf.st_flags & chflags_mask)!=0)
		{
			restore.log("Error setting BSD flags of file \"" + fn + "\" errno: " + convert(errno), LL_ERROR);
			ret = false;
		}
#else
		timespec tss[2];
		tss[0]=statbuf.st_atim;
		tss[1]=statbuf.st_mtim;

		if(utimensat(0, fn.c_str(), tss, 0)!=0)
		{
            restore.log("Error setting access and modification time of file \""+fn+"\" errno: "+convert(errno), LL_ERROR);
            ret = false;
        }
#endif

        return ret;
    }

    bool get_xattr_keys(const std::string& fn, std::vector<std::string>& keys)
    {
        while(true)
        {
            ssize_t bufsize;
            bufsize = llistxattr(fn.c_str(), nullptr, 0);

            if(bufsize==-1)
            {
				if (errno == EOPNOTSUPP)
				{
					return true;
				}
#ifdef __APPLE__
				if (errno == EPERM)
				{
					//fs object does not support extended attributes
					return true;
				}
#endif
                Server->Log("Error getting extended attribute list of file "+fn+" errno: "+convert(errno), LL_ERROR);
                return false;
            }

            std::string buf;
            buf.resize(bufsize);

            bufsize = llistxattr(fn.c_str(), &buf[0], buf.size());

            if(bufsize==-1 && errno==ERANGE)
            {
                Server->Log("Extended attribute list size increased. Retrying...", LL_DEBUG);
                continue;
            }

            if(bufsize==-1)
            {
                Server->Log("Error getting extended attribute list of file "+fn+" errno: "+convert(errno)+" (2)", LL_ERROR);
                return false;
            }

            Tokenize(buf, keys, std::string(1, '\0'));

            return true;
        }
    }

    bool remove_all_xattr(const std::string& fn)
    {
        std::vector<std::string> keys;
        if(!get_xattr_keys(fn, keys))
        {
            return false;
        }

		bool ret = true;
        for(size_t i=0;i<keys.size();++i)
        {
			if (keys[i].empty())
			{
				continue;
			}

            if(lremovexattr(fn.c_str(), keys[i].c_str())!=0)
            {
                Server->Log("Error removing xattr "+keys[i]+" from "+fn+" errno: "+convert(errno), LL_ERROR);
				ret = false;
            }
        }

        return ret;
    }
}

bool FileMetadataDownloadThread::applyOsMetadata( IFile* metadata_f, const std::string& output_fn)
{
    int64 unix_magic_and_size[2];
	unsigned int data_checksum = urb_adler32(0, nullptr, 0);

    if(metadata_f->Read(reinterpret_cast<char*>(unix_magic_and_size), sizeof(unix_magic_and_size))!=sizeof(unix_magic_and_size))
    {
        restore.log("Error reading  \"" + metadata_f->getFilename() + "\"", LL_ERROR);
        has_error=true;
        return false;
    }

	data_checksum = urb_adler32(data_checksum, reinterpret_cast<char*>(unix_magic_and_size), sizeof(unix_magic_and_size));

    if(unix_magic_and_size[1]!=unix_meta_magic)
    {
        restore.log("Unix metadata magic wrong in  \"" + metadata_f->getFilename() + "\"", LL_ERROR);
        has_error=true;
        return false;
    }

	_u32 stat_data_size;
	if(metadata_f->Read(reinterpret_cast<char*>(&stat_data_size), sizeof(_u32))!=sizeof(_u32))
	{
		restore.log("Error reading stat data size from \"" + metadata_f->getFilename() + "\"", LL_ERROR);
		return false;
	}

	data_checksum = urb_adler32(data_checksum, reinterpret_cast<char*>(&stat_data_size), sizeof(_u32));

	stat_data_size = little_endian(stat_data_size);

	if(stat_data_size<1)
	{
		restore.log("Stat data size from \"" + metadata_f->getFilename() + "\" is zero", LL_ERROR);
		return false;
	}

    char version=-1;    

    std::vector<char> stat_data;
	stat_data.resize(stat_data_size);
    if(metadata_f->Read(stat_data.data(), stat_data.size())!=stat_data.size())
    {
        restore.log("Error reading unix metadata from \"" + metadata_f->getFilename() + "\"", LL_ERROR);
        return false;
    }

	data_checksum = urb_adler32(data_checksum, stat_data.data(), stat_data.size());

    CRData read_stat(stat_data.data(), stat_data.size());

	if(!read_stat.getChar(&version) || version!=1)
	{
		restore.log("Unknown unix metadata version +"+convert((int)version)+" in \"" + metadata_f->getFilename() + "\"", LL_ERROR);
		return false;
	}

	std::string symlink_target;
	struct stat64 statbuf;
    unserialize_stat_buf(read_stat, statbuf, symlink_target);

    if(!restore_stat_buf(restore, statbuf, output_fn, symlink_target))
    {
        restore.log("Error setting unix metadata of "+output_fn, LL_ERROR);
    }

    if(!remove_all_xattr(output_fn))
    {
        restore.log("Error removing xattrs of "+ output_fn, LL_ERROR);
    }

    int64 num_eattr_keys;
    if(metadata_f->Read(reinterpret_cast<char*>(&num_eattr_keys), sizeof(num_eattr_keys))!=sizeof(num_eattr_keys))
    {
        restore.log("Error reading eattr num from \"" + metadata_f->getFilename() + "\"", LL_ERROR);
        return false;
    }

	data_checksum = urb_adler32(data_checksum, reinterpret_cast<char*>(&num_eattr_keys), sizeof(num_eattr_keys));

    num_eattr_keys = little_endian(num_eattr_keys);

    for(int64 i=0;i<num_eattr_keys;++i)
    {
        unsigned int key_size;
        if(metadata_f->Read(reinterpret_cast<char*>(&key_size), sizeof(key_size))!=sizeof(key_size))
        {
            restore.log("Error reading eattr key size from \"" + metadata_f->getFilename() + "\"", LL_ERROR);
            return false;
        }

		data_checksum = urb_adler32(data_checksum, reinterpret_cast<char*>(&key_size), sizeof(key_size));

        key_size = little_endian(key_size);

		if (key_size > 1 * 1024 * 1024)
		{
			restore.log("Eattr key " + convert(i) + " for  \"" + output_fn + "\" too large with size " + PrettyPrintBytes(key_size)
				+ " in \"" + metadata_f->getFilename() + "\"", LL_ERROR);
			return false;
		}

        std::string eattr_key;
        eattr_key.resize(key_size);

        if(metadata_f->Read(&eattr_key[0], eattr_key.size())!=eattr_key.size())
        {
            restore.log("Error reading eattr key from \"" + metadata_f->getFilename() + "\"", LL_ERROR);
            return false;
        }

		data_checksum = urb_adler32(data_checksum, &eattr_key[0], eattr_key.size());

        unsigned int val_size;
        if(metadata_f->Read(reinterpret_cast<char*>(&val_size), sizeof(val_size))!=sizeof(val_size))
        {
            restore.log("Error reading eattr value size from \"" + metadata_f->getFilename() + "\"", LL_ERROR);
            return false;
        }

		data_checksum = urb_adler32(data_checksum, reinterpret_cast<char*>(&val_size), sizeof(val_size));

        val_size = little_endian(val_size);

		if (val_size == UINT_MAX)
		{
			continue;
		}

		if (val_size > 10 * 1024 * 1024)
		{
			restore.log("Eattr value for eattr \"" + eattr_key + "\" for \"" + output_fn + "\" too large with size " + PrettyPrintBytes(val_size)
				+ " in \"" + metadata_f->getFilename() + "\"", LL_ERROR);
			return false;
		}

        std::string eattr_val;
        eattr_val.resize(val_size);

        if(metadata_f->Read(&eattr_val[0], eattr_val.size())!=eattr_val.size())
        {
            restore.log("Error reading eattr value from \"" + metadata_f->getFilename() + "\"", LL_ERROR);
            return false;
        }

		data_checksum = urb_adler32(data_checksum, &eattr_val[0], eattr_val.size());

        if(lsetxattr(output_fn.c_str(), eattr_key.c_str(), eattr_val.data(), eattr_val.size(), 0)!=0)
        {
            restore.log("Error setting xattr "+eattr_key+" of "+ output_fn +" errno: "+convert(errno), LL_ERROR);
        }
    }

	unsigned int read_data_checksum =0;
	if(metadata_f->Read(reinterpret_cast<char*>(&read_data_checksum), sizeof(read_data_checksum))!=sizeof(read_data_checksum))
	{
		restore.log( "Error saving metadata. Data checksum could not be read.", LL_ERROR);
		return false;
	}

	if(little_endian(read_data_checksum)!=data_checksum)
	{
		restore.log("Error saving metadata. Data checksum wrong.", LL_ERROR);
		return false;
	}

    return true;
}

#endif //_WIN32


void FileMetadataDownloadThread::shutdown(bool log_error)
{
	error_logging = log_error;
	fc.Shutdown();
}


int64 FileMetadataDownloadThread::getTransferredBytes()
{
	return fc.getTransferredBytes();
}



} //namespace client

