#include "PhashLoad.h"
#include "PhashLoad.h"
#include "PhashLoad.h"
#include "PhashLoad.h"
#include "PhashLoad.h"
#include "../urbackupcommon/fileclient/FileClient.h"
#include "../Interface/Server.h"
#include "../Interface/File.h"
#include "ClientMain.h"
#include "server_settings.h"
#include "database.h"
#include "../common/data.h"

extern std::string server_token;

PhashLoad::PhashLoad(FileClient* fc,
	logid_t logid, std::string async_id)
	: has_error(false), fc(fc),
	logid(logid), async_id(async_id),
	phash_file_pos(0), phash_file(NULL), eof(false),
	has_timeout_error(false), orig_progress_log_callback(fc->getProgressLogCallback())
{
}

PhashLoad::~PhashLoad()
{
	ScopedDeleteFile del_file(phash_file);
}

void PhashLoad::operator()()
{
	fc->setProgressLogCallback(NULL);
	fc->setNoFreeSpaceCallback(NULL);

	std::string cfn = "SCRIPT|phash_{9c28ff72-5a74-487b-b5e1-8f1c96cd0cf4}/phash_" + async_id + "|0|0|" + server_token;
	std::unique_ptr<IFsFile> phash_file_dl(Server->openTemporaryFile());

	if (phash_file_dl.get() == NULL)
	{
		ServerLogger::Log(logid, "Error opening random file for parallel hash load. "+os_last_error_str(), LL_ERROR);
		has_error = true;
		return;
	}

	int mode = MODE_READ_SEQUENTIAL;
#ifdef _WIN32
	mode = MODE_READ_DEVICE;
#endif
	phash_file = Server->openFile(phash_file_dl->getFilename(), mode);

	if(phash_file == NULL)
	{
		ScopedDeleteFn del_file(phash_file_dl->getFilename());
		ServerLogger::Log(logid, "Error re-opening random file for parallel hash load. " + os_last_error_str(), LL_ERROR);
		has_error = true;
		return;
	}

	fc->setReconnectTries(5000);
	_u32 rc = fc->GetFile(cfn, phash_file_dl.get(), true, false, 0, true, 0);

	if (rc != ERR_SUCCESS)
	{
		ServerLogger::Log(logid, "Error during parallel hash load: "+fc->getErrorString(rc), LL_ERROR);
		has_error = true;

		if (rc == ERR_CONN_LOST || rc == ERR_TIMEOUT)
		{
			has_timeout_error = true;
		}

		return;
	}
	else
	{
		ServerLogger::Log(logid, "Parallel hash loading finished successfully", LL_INFO);
		fc->FinishScript(cfn);
	}

	eof = true;
}

bool PhashLoad::getHash(int64 file_id, std::string & hash)
{
	while (true)
	{
		while (phash_file == NULL
			|| phash_file_pos + static_cast<int64>(sizeof(_u16)) > phash_file->Size())
		{
			if ((has_error || eof)
				&& phash_file_pos + static_cast<int64>(sizeof(_u16)) > phash_file->Size())
			{
				ServerLogger::Log(logid, "Getting parallel file hash record size of id " + convert(file_id) + " from " + phash_file->getFilename() + " failed. "
					+ (eof ? "EOF." : "Had error."), LL_ERROR);
				return false;
			}
			Server->wait(1000);
		}

		_u16 msgsize;
		if (phash_file->Read(phash_file_pos, reinterpret_cast<char*>(&msgsize), sizeof(msgsize)) != sizeof(msgsize))
		{
			ServerLogger::Log(logid, "Error reading record size (" + convert(sizeof(msgsize)) + 
				" bytes) from parallel hash file " + phash_file->getFilename() + ". " + os_last_error_str(), LL_ERROR);
			return false;
		}

		msgsize = little_endian(msgsize);

		while (phash_file_pos + static_cast<int64>(sizeof(_u16)) + msgsize > phash_file->Size())
		{
			if ((has_error || eof)
				&& phash_file_pos + static_cast<int64>(sizeof(_u16)) + msgsize > phash_file->Size())
			{
				ServerLogger::Log(logid, "Getting parallel file hash record data of id " + convert(file_id) + " from " + phash_file->getFilename() + " failed. "
					+ (eof ? "EOF." : "Had error."), LL_ERROR);
				return false;
			}
			Server->wait(1000);
		}

		std::string msgdata = phash_file->Read(phash_file_pos + sizeof(_u16), msgsize);
		if (msgdata.size() != msgsize)
		{
			ServerLogger::Log(logid, "Error reading parallel hash file record (size "+convert(msgsize)+" read only "+convert(msgdata.size())+") "
				"from parallel hash file " + phash_file->getFilename() + ". " + os_last_error_str(), LL_ERROR);
			return false;
		}

		phash_file_pos += sizeof(_u16) + msgsize;

		CRData data(msgdata.data(), msgdata.size());
		char id;
		if (!data.getChar(&id))
			return false;

		if (id == 0)
			continue;

		if (id != 1)
		{
			ServerLogger::Log(logid, "Unknown parallel hash file record id", LL_ERROR);
			return false;
		}

		int64 curr_file_id;
		if (!data.getVarInt(&curr_file_id))
		{
			ServerLogger::Log(logid, "Error reading parallel hash file id", LL_ERROR);
			return false;
		}

		if (!data.getStr2(&hash))
		{
			ServerLogger::Log(logid, "Error reading parallel hash file hash", LL_ERROR);
			return false;
		}

		if (curr_file_id > file_id)
		{
			phash_file_pos -= sizeof(_u16) + msgsize;
			hash.clear();
			ServerLogger::Log(logid, "Current parallel hash position greated than requested. "+convert(curr_file_id)+" > "+convert(file_id), LL_ERROR);
			return false;
		}
		else if (curr_file_id < file_id)
		{
			ServerLogger::Log(logid, "Skip phash for id " + convert(curr_file_id) + " " + base64_encode_dash(hash), LL_DEBUG);
			hash.clear();
			continue;
		}

		ServerLogger::Log(logid, "Phash for id " + convert(file_id) + " is " + base64_encode_dash(hash), LL_DEBUG);

		return true;
	}
}

bool PhashLoad::hasError()
{
	return has_error;
}

void PhashLoad::shutdown()
{
	fc->Shutdown();
}

bool PhashLoad::isDownloading()
{
	return fc->isDownloading();
}

void PhashLoad::setProgressLogEnabled(bool b)
{
	if (b)
	{
		fc->setProgressLogCallback(orig_progress_log_callback);
	}
	else
	{
		fc->setProgressLogCallback(NULL);
	}
}
