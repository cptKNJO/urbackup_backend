/*************************************************************************
*    UrBackup - Client/Server backup system
*    Copyright (C) 2011-2021 Martin Raiber
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

#include "FSImageFactory.h"
#include "../Interface/Server.h"
#include "../Interface/File.h"
#include <memory.h>

#include "fs/ntfs.h"
#ifdef _WIN32
#include "fs/ntfs_win.h"
#define FSNTFS FSNTFSWIN
#endif
#include "fs/unknown.h"
#include "vhdfile.h"
#include "vhdxfile.h"
#include "../stringtools.h"
#ifdef _WIN32
#include <Windows.h>
#include "ImdiskSrv.h"
#else
#include <errno.h>
#endif
#include "partclone.h"
#include "cowfile.h"
#include "ClientBitmap.h"
#include <stdlib.h>
#include "FileWrapper.h"

#ifndef _WIN32
#include "../config.h"
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#endif


#ifdef _WIN32
namespace
{
	LONG GetStringRegKey(HKEY hKey, const std::string &strValueName, std::string &strValue, const std::string &strDefaultValue)
	{
		strValue = strDefaultValue;
		WCHAR szBuffer[8192];
		DWORD dwBufferSize = sizeof(szBuffer);
		ULONG nError;
		std::wstring rval;
		nError = RegQueryValueExW(hKey, Server->ConvertToWchar(strValueName).c_str(), 0, NULL, (LPBYTE)szBuffer, &dwBufferSize);
		if (ERROR_SUCCESS == nError)
		{
			rval.resize(dwBufferSize/sizeof(wchar_t));
			memcpy(const_cast<wchar_t*>(rval.c_str()), szBuffer, dwBufferSize);
			strValue = Server->ConvertFromWchar(rval);
		}
		return nError;
	}
}
#endif

namespace
{
#ifndef _WIN32
        std::string getFolderMount(const std::string& path)
        {
#ifndef HAVE_MNTENT_H
                return std::string();
#else
                FILE *aFile;

                aFile = setmntent("/proc/mounts", "r");
                if (aFile == NULL) {
                        return std::string();
                }
                struct mntent *ent;
                std::string maxmount;
                while (NULL != (ent = getmntent(aFile)))
                {
                        if(path.find(ent->mnt_dir)==0 &&
                                std::string(ent->mnt_dir).size()>maxmount.size())
                        {
                                maxmount = ent->mnt_dir;
                        }
                }
                endmntent(aFile);

                return maxmount;
#endif //HAVE_MNTENT_H
        }
#endif //!_WIN32

	std::string replaceMountPoint(std::string fn, std::string snap_mountpoint)
	{
#ifdef _WIN32
		return snap_mountpoint + "/" + fn;
#else
		std::string mountpoint = getFolderMount(fn);
		if(fn.size()>=mountpoint.size())
		{
			fn.erase(0, mountpoint.size());
			return snap_mountpoint + "/" + fn;
		}
		else
		{
			return snap_mountpoint + "/" + fn;
		}
#endif
	}



	std::vector<std::string> getSwapFiles(std::string snap_mountpoint)
	{
		std::string swaps_str = getStreamFile("/proc/swaps");

		std::vector<std::string> ret;
		std::vector<std::string> lines;
		Tokenize(swaps_str, lines, "\n");
		for(size_t i=1;i<lines.size();++i)
		{
			std::vector<std::string> cols;
			std::string line = lines[i];
			std::string line_tr;
			int tr_state=0;
			for(size_t j=0;j<line.size();++j)
			{
				if(tr_state==0)
				{
					if(line[j]==' ' || line[j]=='\t')
					{
						line_tr+=' ';
						tr_state=1;
					}
					else
						line_tr+=line[j];
				}
				else
				{
					if(line[j]!=' ' && line[j]!='\t')
					{
						line_tr+=line[j];
						tr_state=0;
					}
				}
			}
			Tokenize(line_tr, cols, " ");
			if(cols.size()>4)
			{
				if(cols[1]=="file")
				{
					std::string fn = cols[0];
					ret.push_back(replaceMountPoint(fn, snap_mountpoint));
				}
			}
		}

		return ret;
	}
}

namespace
{
#pragma pack(push)
#pragma pack(1)
	struct MBRPartition
	{
		unsigned char boot_flag;
		unsigned char chs_begin[3];
		unsigned char sys_type;
		unsigned char chs_end[3];
		unsigned int start_sector;
		unsigned int nr_sector;
	};
#pragma pack(pop)

	std::vector<IFSImageFactory::SPartition> readPartitionsMBR(const std::string& mbr)
	{
		const MBRPartition* partitions = reinterpret_cast<const MBRPartition*>(&mbr[446]);
		const int64 c_sector_size = 512;

		std::vector<IFSImageFactory::SPartition> ret;
		for (size_t i = 0; i < 4; ++i)
		{
			const MBRPartition* cpart = &partitions[i];
			if (cpart->sys_type == 0)
			{
				continue;
			}

			IFSImageFactory::SPartition part;
			part.offset = cpart->start_sector*c_sector_size;
			part.length = cpart->nr_sector*c_sector_size;
			ret.push_back(part);
		}

		return ret;
	}

#pragma pack(push)
#pragma pack(1)
	struct EfiHeader
	{
		uint64 signature;
		_u32 revision;
		_u32 header_size;
		_u32 header_crc;
		_u32 reserved;
		int64 current_lba;
		int64 backup_lba;
		int64 first_lba;
		int64 last_lba;
		char disk_guid[16];
		int64 partition_table_lba;
		_u32 num_parition_entries;
		_u32 partition_entry_size;
		_u32 partition_table_crc;
	};

	struct GPTPartition
	{
		char partition_type_guid[16];
		char unique_partition_guid[16];
		int64 first_lba;
		int64 last_lba;
		uint64 flags;
		char name[72];
	};
#pragma pack(pop)

	std::vector<IFSImageFactory::SPartition> readPartitionsGPT(IFile* dev, const std::string& gpt_header)
	{
		const EfiHeader* efi_header = reinterpret_cast<const EfiHeader*>(&gpt_header[0]);
		const int64 c_sector_size = 512;

		int64 table_pos = efi_header->partition_table_lba*c_sector_size;

		char partition_type_guid_empty[16] = {};

		std::vector<IFSImageFactory::SPartition> ret;
		for (_u32 i = 0; i < efi_header->num_parition_entries; ++i)
		{
			std::string table_entry = dev->Read(table_pos, efi_header->partition_entry_size);
			if (table_entry.size() < sizeof(GPTPartition))
			{
				continue;
			}
			table_pos += efi_header->partition_entry_size;

			const GPTPartition* cpart = reinterpret_cast<const GPTPartition*>(table_entry.data());

			if (cpart->first_lba == 0
				&& cpart->last_lba == 0
				&& cpart->flags == 0
				&& memcmp(cpart->partition_type_guid, partition_type_guid_empty, sizeof(partition_type_guid_empty)) == 0)
			{
				continue;
			}

			IFSImageFactory::SPartition new_part;
			new_part.offset = cpart->first_lba*c_sector_size;
			new_part.length = (cpart->last_lba - cpart->first_lba + 1)*c_sector_size;
			ret.push_back(new_part);
		}
		return ret;
	}

	std::vector<IFSImageFactory::SPartition> readPartitionsGPT(const std::string& gpt_table, const std::string& gpt_header)
	{
		const EfiHeader* efi_header = reinterpret_cast<const EfiHeader*>(&gpt_header[0]);
		const int64 c_sector_size = 512;

		char partition_type_guid_empty[16] = {};

		std::vector<IFSImageFactory::SPartition> ret;
		for (_u32 i = 0; i < efi_header->num_parition_entries; ++i)
		{
			const GPTPartition* cpart = reinterpret_cast<const GPTPartition*>(gpt_table.data()+ efi_header->partition_entry_size*i);

			if (cpart->first_lba == 0
				&& cpart->last_lba == 0
				&& cpart->flags == 0
				&& memcmp(cpart->partition_type_guid, partition_type_guid_empty, sizeof(partition_type_guid_empty)) == 0)
			{
				continue;
			}

			IFSImageFactory::SPartition new_part;
			new_part.offset = cpart->first_lba*c_sector_size;
			new_part.length = (cpart->last_lba - cpart->first_lba + 1)*c_sector_size;
			ret.push_back(new_part);
		}
		return ret;
	}
}


void PrintInfo(IFilesystem *fs)
{
	Server->Log("FSINFO: blocksize="+convert(fs->getBlocksize())+" size="+convert(fs->getSize())+" has_error="+convert(fs->hasError())+" used_space="+convert(fs->calculateUsedSpace())+" type="+fs->getType(), LL_DEBUG);
}

IFilesystem *FSImageFactory::createFilesystem(const std::string &pDevOrig, EReadaheadMode read_ahead,
	bool background_priority, std::string orig_letter, IFsNextBlockCallback* next_block_callback)
{
	std::string pDev = pDevOrig;
#ifndef _WIN32
	if(read_ahead==EReadaheadMode_Overlapped)
	{
		read_ahead = EReadaheadMode_None;
	}

	pDev = trim(getFile(pDevOrig+"-dev"));
	if(pDev.empty())
	{
		Server->Log("Error reading device file name from "+pDevOrig+"-dev", LL_ERROR);
		return nullptr;
	}
#endif

	IFile *dev = Server->openFile(pDev, MODE_READ_DEVICE);

	if(dev==nullptr)
	{
		int last_error;
#ifdef _WIN32
		last_error=GetLastError();
#else
		last_error=errno;
#endif
		Server->Log("Error opening device file ("+pDev+") Errorcode: "+convert(last_error), LL_ERROR);
		return nullptr;
	}
	char buffer[4096];
	_u32 rc=dev->Read(buffer, 4096);
	if(rc!=4096)
	{
		int last_error;
#ifdef _WIN32
		last_error = GetLastError();
#else
		last_error = errno;
#endif
		Server->Log("Error reading data from device ("+pDev+"). Errorcode: " + convert(last_error), LL_ERROR);
		return nullptr;
	}

	Server->destroy(dev);

	if(isNTFS(buffer) )
	{
		Server->Log("Filesystem type is ntfs ("+pDev+")", LL_DEBUG);

		FSNTFS *fs=new FSNTFS(pDev, read_ahead, background_priority, next_block_callback);


#ifdef _WIN32
		if(next(orig_letter, 0, "C"))
		{
			fs->excludeFile(pDev+"\\HIBERFIL.SYS");
		}

		HKEY hKey;
		LONG lRes = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management", 0, KEY_READ, &hKey);
		if(lRes == ERROR_SUCCESS)
		{
			std::string tfiles;
			lRes = GetStringRegKey(hKey, "ExistingPageFiles", tfiles, "");
			if(lRes == ERROR_SUCCESS)
			{
				std::vector<std::string> toks;
				std::string sep;
				sep.resize(1);
				sep[0]=0;
				Tokenize(tfiles, toks, sep);
				for(size_t i=0;i<toks.size();++i)
				{
					toks[i]=trim(toks[i].c_str());

					if(toks[i].empty())
						continue;

					toks[i]=trim(toks[i]);

					if(toks[i].find("\\??\\")==0)
					{
						toks[i].erase(0, 4);
					}

					toks[i]=strlower(toks[i]);

					if(next(toks[i], 0, strlower(orig_letter)))
					{
						std::string fs_path=orig_letter;
						if(fs_path.find(":")==std::string::npos)
						{
							fs_path+=":";
						}
						if(fs_path.find("\\")!=std::string::npos)
						{
							fs_path = fs_path.substr(0, fs_path.size()-1);
						}
						if(toks[i].size()>fs_path.size())
						{
							std::string pagefile_shadowcopy = toks[i].substr(fs_path.size());
							fs->excludeFile(pDev+pagefile_shadowcopy);
						}
					}
				}
			}
		}

		char* systemdrive = NULL;
		size_t bufferCount;
		if (_dupenv_s(&systemdrive, &bufferCount, "SystemDrive") == 0)
		{
			std::string s_systemdrive = std::string(systemdrive);

			if (!s_systemdrive.empty()
				&& next(orig_letter, 0, s_systemdrive.substr(0, 1)) )
			{
				fs->excludeFile(pDev + "\\swapfile.sys");
			}
		}
		free(systemdrive);

		//Exclude shadow storage files
		if(pDev.find("HarddiskVolumeShadowCopy") != std::string::npos)
		{
			fs->excludeFiles(pDev + "\\System Volume Information", "{3808876b-c176-4e48-b7ae-04046e6cc752}");
		}
#endif
		
		/*
		int64 idx=0;
		while(idx<fs->getSize()/fs->getBlocksize())
		{
			std::string b1;
			std::string b2;
			int64 idx_start=idx;
			for(size_t i=0;i<100;++i)
			{
				b1+=convert((int)fs->readBlock(idx, NULL));
				b2+=convert((int)fs2->readBlock(idx, NULL));
				++idx;
			}
			if(b1!=b2)
			{
				Server->Log(convert(idx_start)+" fs1: "+b1, LL_DEBUG);
				Server->Log(convert(idx_start)+" fs2: "+b2, LL_DEBUG);
			}
		}*/

		if(fs->hasError())
		{
			Server->Log("NTFS has error", LL_WARNING);
			delete fs;

			Server->Log("Unknown filesystem type", LL_DEBUG);
			FSUnknown *fs2=new FSUnknown(pDev, read_ahead, background_priority, next_block_callback);
			if(fs2->hasError())
			{
				delete fs2;
				return nullptr;
			}
			PrintInfo(fs2);
			return fs2;
		}
		PrintInfo(fs);
		return fs;
	}
#ifdef _WIN32
	else
	{
		Server->Log("Unknown filesystem type", LL_DEBUG);
		FSUnknown *fs=new FSUnknown(pDev, read_ahead, background_priority, next_block_callback);
		if(fs->hasError())
		{
			delete fs;
			return NULL;
		}
		PrintInfo(fs);
		return fs;
	}
#else
	else
	{
		IFilesystem* fs = new Partclone(pDev, read_ahead, background_priority, next_block_callback);
		if (fs->hasError())
		{
			delete fs;
			fs = new FSUnknown(pDev, read_ahead, background_priority, next_block_callback);
			if (fs->hasError())
			{
				delete fs;
				return nullptr;
			}
		}
		PrintInfo(fs);

		std::vector<std::string> swap_files = getSwapFiles(pDevOrig);
		for(size_t i=0;i<swap_files.size();++i)
		{
			fs->excludeFile(swap_files[i]);
		}

		fs->excludeFiles(pDevOrig, ".datto_3d41c58e-6724-4d47-8981-11c766a08a24_");
		fs->excludeFiles(pDevOrig, ".overlay_2fefd007-3e48-4162-b2c6-45ccdda22f37_");
		

		return fs;
	}
#endif
}

bool FSImageFactory::isNTFS(char *buffer)
{
	if(buffer[3]=='N' && buffer[4]=='T' && buffer[5]=='F' && buffer[6]=='S')
	{
		return true;
	}
	else
	{
		return false;
	}
}

IVHDFile *FSImageFactory::createVHDFile(const std::string &fn, bool pRead_only, uint64 pDstsize,
	unsigned int pBlocksize, bool fast_mode, ImageFormat format, size_t n_compress_threads)
{
	switch(format)
	{
	case ImageFormat_VHD:
	case ImageFormat_CompressedVHD:
		return new VHDFile(fn, pRead_only, pDstsize, pBlocksize, fast_mode, format!=ImageFormat_VHD);
	case ImageFormat_VHDX:
	case ImageFormat_CompressedVHDX:
		return new VHDXFile(fn, pRead_only, pDstsize, pBlocksize, fast_mode, format != ImageFormat_VHDX);
	case ImageFormat_RawCowFile:
#if !defined(__APPLE__)
		return new CowFile(fn, pRead_only, pDstsize);
#else
		return NULL;
#endif
	}
	return nullptr;
}

IVHDFile *FSImageFactory::createVHDFile(const std::string &fn, const std::string &parent_fn,
	bool pRead_only, bool fast_mode, ImageFormat format, uint64 pDstsize, size_t n_compress_threads)
{
	switch(format)
	{
	case ImageFormat_VHD:
	case ImageFormat_CompressedVHD:
		return new VHDFile(fn, parent_fn, pRead_only, fast_mode, format!=ImageFormat_VHD, pDstsize);
	case ImageFormat_VHDX:
	case ImageFormat_CompressedVHDX:
		return new VHDXFile(fn, parent_fn, pRead_only, fast_mode, format != ImageFormat_VHDX, pDstsize);
	case ImageFormat_RawCowFile:
#if !defined(__APPLE__)
		return new CowFile(fn, parent_fn, pRead_only, pDstsize);
#else
		return NULL;
#endif
	}

	return nullptr;
}

void FSImageFactory::destroyVHDFile(IVHDFile *vhd)
{
	delete vhd;
}

IReadOnlyBitmap * FSImageFactory::createClientBitmap(const std::string & fn)
{
	return new ClientBitmap(fn);
}

IReadOnlyBitmap * FSImageFactory::createClientBitmap(IFile * bitmap_file)
{
	return new ClientBitmap(bitmap_file);
}

bool FSImageFactory::initializeImageMounting()
{
#ifdef _WIN32
	if (ImdiskSrv::installed())
	{
		Server->createThread(new ImdiskSrv, "imdisk srv");
		return true;
	}
	else
	{
		return false;
	}
#else
	std::string mount_helper = Server->getServerParameter("mount_helper");
	if (mount_helper.empty())
	{
		mount_helper = "urbackup_mount_helper";
	}
	int rc = system((mount_helper + " test").c_str());
	return rc == 0;
#endif
}

std::vector<IFSImageFactory::SPartition> FSImageFactory::readPartitions(IVHDFile * vhd, int64 offset, bool& gpt_style)
{
	FileWrapper dev(vhd, offset);
	return readPartitions(&dev, gpt_style);
}

std::vector<FSImageFactory::SPartition> FSImageFactory::readPartitions(IFile* dev, bool& gpt_style)
{
	gpt_style = false;

	std::string mbr = dev->Read(0LL, 512, nullptr);
	std::string gpt_header = dev->Read(512LL, 512, nullptr);

	if (next(gpt_header, 0, "EFI PART"))
	{
		gpt_style = true;
		return readPartitionsGPT(dev, gpt_header);
	}
	else if (mbr.size() >= 512
		&& static_cast<unsigned char>(mbr[510]) == 0x55
		&& static_cast<unsigned char>(mbr[511]) == 0xAA)
	{
		return readPartitionsMBR(mbr);
	}

	return std::vector<IFSImageFactory::SPartition>();
}

std::vector<IFSImageFactory::SPartition> FSImageFactory::readPartitions(const std::string& mbr,
	const std::string& gpt_header, const std::string& gpt_table, bool& gpt_style)
{
	gpt_style = false;

	if (next(gpt_header, 0, "EFI PART"))
	{
		return readPartitionsGPT(gpt_table, gpt_header);
		gpt_style = true;
	}
	else if (mbr.size() >= 512
		&& static_cast<unsigned char>(mbr[510]) == 0x55
		&& static_cast<unsigned char>(mbr[511]) == 0xAA)
	{
		return readPartitionsMBR(mbr);
	}

	return std::vector<IFSImageFactory::SPartition>();
}
