#pragma once
#include "../../Interface/Database.h"

class ServerBackupDao
{
public:
	ServerBackupDao(IDatabase *db);
	~ServerBackupDao();

	static int num_issues_no_backuppaths;

	//@-SQLGenFunctionsBegin
	struct CondInt
	{
		bool exists;
		int value;
	};
	struct CondInt64
	{
		bool exists;
		int64 value;
	};
	struct CondString
	{
		bool exists;
		std::string value;
	};
	struct SClientName
	{
		bool exists;
		std::string virtualmain;
		std::string name;
	};
	struct SDuration
	{
		int64 indexing_time_ms;
		int64 duration;
	};
	struct SFileBackupInfo
	{
		bool exists;
		int64 id;
		int clientid;
		int64 backuptime;
		int incremental;
		std::string path;
		int complete;
		int64 running;
		int64 size_bytes;
		int done;
		int archived;
		int64 archive_timeout;
		int64 size_calculated;
		int resumed;
		int64 indexing_time_ms;
		int tgroup;
	};
	struct SImageBackup
	{
		bool exists;
		int64 id;
		int incremental;
		std::string path;
		int64 duration;
	};
	struct SLastIncremental
	{
		bool exists;
		int incremental;
		std::string path;
		int resumed;
		int complete;
		int id;
		int incremental_ref;
		int deletion_protected;
	};
	struct SMountedImage
	{
		bool exists;
		int id;
		int backupid;
		std::string path;
		int64 mounttime;
		int partition;
		int clientid;
	};
	struct SReportSettings
	{
		bool exists;
		std::string report_mail;
		int report_loglevel;
		int report_sendonly;
	};
	struct SSetting
	{
		bool exists;
		std::string value;
		std::string value_client;
		int use;
		int64 use_last_modified;
	};


	void addToOldBackupfolders(const std::string& backupfolder);
	std::vector<std::string> getOldBackupfolders(void);
	std::vector<std::string> getDeletePendingClientNames(void);
	CondString getGroupName(int groupid);
	CondInt getClientGroup(int clientid);
	SSetting getServerSetting(const std::string& key, int clientid);
	SClientName getVirtualMainClientname(int clientid);
	std::vector<SDuration> getLastIncrementalDurations(int clientid);
	std::vector<SDuration> getLastFullDurations(int clientid);
	CondString getClientSetting(const std::string& key, int clientid);
	std::vector<int> getClientIds(void);
	std::vector<int> getClientsByUid(const std::string& uid);
	CondString getClientUid(int id);
	void updateClientUid(const std::string& uid, int clientid);
	void deleteClient(int clientid);
	void changeClientName(const std::string& name, int id);
	void changeClientNameWithVirtualmain(const std::string& name, const std::string& virtualmain, int id);
	void addClientMoved(const std::string& from_name, const std::string& to_name);
	std::vector<std::string> getClientMovedLimit5(const std::string& to_name);
	std::vector<std::string> getClientMovedFrom(const std::string& from_name);
	CondString getSetting(int clientid, const std::string& key);
	int hasFileBackups(int clientid);
	void insertSetting(const std::string& key, const std::string& value, int clientid);
	void updateSetting(const std::string& value, const std::string& key, int clientid);
	CondString getMiscValue(const std::string& tkey);
	void addMiscValue(const std::string& tkey, const std::string& tvalue);
	void delMiscValue(const std::string& tkey);
	void setClientUsedFilebackupSize(int64 bytes_used_files, int id);
	bool newFileBackup(int incremental, int clientid, const std::string& path, int resumed, int64 indexing_time_ms, int tgroup, int incremental_ref, int deletion_protected);
	void updateFileBackupRunning(int backupid);
	void setFileBackupDone(int backupid);
	void setFileBackupSynced(int backupid);
	SLastIncremental getLastIncrementalFileBackup(int clientid, int tgroup);
	SLastIncremental getLastIncrementalCompleteFileBackup(int clientid, int tgroup);
	void updateFileBackupSetComplete(int backupid);
	void saveBackupLog(int clientid, int errors, int warnings, int infos, int image, int incremental, int resumed, int restore);
	void saveBackupLogData(int64 logid, const std::string& data);
	std::vector<int> getMailableUserIds(void);
	CondString getUserRight(int clientid, const std::string& t_domain);
	SReportSettings getUserReportSettings(int userid);
	CondString formatUnixtime(int64 unixtime);
	SImageBackup getLastFullImage(int clientid, int image_version, const std::string& letter);
	SImageBackup getLastImage(int clientid, int image_version, const std::string& letter);
	bool newImageBackup(int clientid, const std::string& path, int incremental, int incremental_ref, int image_version, const std::string& letter, int64 backuptime);
	void setImageSize(int64 size_bytes, int backupid);
	void addImageSizeToClient(int clientid, int64 add_size);
	void setImageBackupSynctime(int backupid);
	void setImageBackupComplete(int backupid);
	void setImageBackupIncomplete(int backupid);
	void updateImageBackupRunning(int backupid);
	void saveImageAssociation(int img_id, int assoc_id);
	void updateClientLastImageBackup(int backupid, int clientid);
	void updateClientNumIssues(int last_filebackup_issues, int clientid);
	void updateClientLastFileBackup(int backupid, int last_filebackup_issues, int clientid);
	void updateClientOsAndClientVersion(const std::string& os_simple, const std::string& os_version, const std::string& client_version, int capa, int clientid);
	void deleteAllUsersOnClient(int clientid);
	void addUserOnClient(int clientid, const std::string& username);
	void addClientToken(int clientid, const std::string& token);
	void addUserToken(const std::string& username, int clientid, const std::string& token);
	void addUserTokenWithGroup(const std::string& username, int clientid, const std::string& token, const std::string& tgroup);
	CondInt64 hasRecentFullOrIncrFileBackup(const std::string& backup_interval_full, int clientid, const std::string& backup_interval_incr, int tgroup);
	CondInt64 hasRecentIncrFileBackup(const std::string& backup_interval, int clientid, int tgroup);
	CondInt64 hasRecentFullOrIncrImageBackup(const std::string& backup_interval_full, int clientid, const std::string& backup_interval_incr, int image_version, const std::string& letter);
	CondInt64 hasRecentIncrImageBackup(const std::string& backup_interval, int clientid, int image_version, const std::string& letter);
	void addRestore(int clientid, const std::string& path, const std::string& identity, int image, const std::string& letter);
	CondString getRestorePath(int64 restore_id, int clientid);
	CondString getRestoreIdentity(int64 restore_id, int clientid);
	void setRestoreDone(int success, int64 restore_id);
	void deleteRestore(int64 restore_id);
	SFileBackupInfo getFileBackupInfo(int backupid);
	void setVirtualMainClient(const std::string& virtualmain, int64 clientid);
	void deleteUsedAccessTokens(int clientid);
	CondInt hasUsedAccessToken(const std::string& tokenhash);
	void addUsedAccessToken(int clientid, const std::string& tokenhash);
	CondString getClientnameByImageid(int backupid);
	CondInt getClientidByImageid(int backupid);
	CondInt getImageMounttime(int backupid);
	void updateImageMounted(int64 id);
	void addImageMounted(int backupid, int partition);
	void delImageMounted(int64 id);
	SMountedImage getMountedImage(int backupid, int partition);
	SMountedImage getImageInfo(int backupid);
	std::vector<SMountedImage> getOldMountedImages(int64 times);
	void setCapa(int capa, int clientid);
	CondInt getCapa(int clientid);
	CondInt getClientWithHashes(int clientid);
	void updateClientWithHashes(int with_hashes, int clientid);
	//@-SQLGenFunctionsEnd

	void updateOrInsertSetting(int clientid, const std::string& key, const std::string& value);

private:
	ServerBackupDao(ServerBackupDao& other) {}
	void operator=(ServerBackupDao& other) {}

	void prepareQueries(void);
	void destroyQueries(void);

	//@-SQLGenVariablesBegin
	IQuery* q_addToOldBackupfolders;
	IQuery* q_getOldBackupfolders;
	IQuery* q_getDeletePendingClientNames;
	IQuery* q_getGroupName;
	IQuery* q_getClientGroup;
	IQuery* q_getServerSetting;
	IQuery* q_getVirtualMainClientname;
	IQuery* q_getLastIncrementalDurations;
	IQuery* q_getLastFullDurations;
	IQuery* q_getClientSetting;
	IQuery* q_getClientIds;
	IQuery* q_getClientsByUid;
	IQuery* q_getClientUid;
	IQuery* q_updateClientUid;
	IQuery* q_deleteClient;
	IQuery* q_changeClientName;
	IQuery* q_changeClientNameWithVirtualmain;
	IQuery* q_addClientMoved;
	IQuery* q_getClientMovedLimit5;
	IQuery* q_getClientMovedFrom;
	IQuery* q_getSetting;
	IQuery* q_hasFileBackups;
	IQuery* q_insertSetting;
	IQuery* q_updateSetting;
	IQuery* q_getMiscValue;
	IQuery* q_addMiscValue;
	IQuery* q_delMiscValue;
	IQuery* q_setClientUsedFilebackupSize;
	IQuery* q_newFileBackup;
	IQuery* q_updateFileBackupRunning;
	IQuery* q_setFileBackupDone;
	IQuery* q_setFileBackupSynced;
	IQuery* q_getLastIncrementalFileBackup;
	IQuery* q_getLastIncrementalCompleteFileBackup;
	IQuery* q_updateFileBackupSetComplete;
	IQuery* q_saveBackupLog;
	IQuery* q_saveBackupLogData;
	IQuery* q_getMailableUserIds;
	IQuery* q_getUserRight;
	IQuery* q_getUserReportSettings;
	IQuery* q_formatUnixtime;
	IQuery* q_getLastFullImage;
	IQuery* q_getLastImage;
	IQuery* q_newImageBackup;
	IQuery* q_setImageSize;
	IQuery* q_addImageSizeToClient;
	IQuery* q_setImageBackupSynctime;
	IQuery* q_setImageBackupComplete;
	IQuery* q_setImageBackupIncomplete;
	IQuery* q_updateImageBackupRunning;
	IQuery* q_saveImageAssociation;
	IQuery* q_updateClientLastImageBackup;
	IQuery* q_updateClientNumIssues;
	IQuery* q_updateClientLastFileBackup;
	IQuery* q_updateClientOsAndClientVersion;
	IQuery* q_deleteAllUsersOnClient;
	IQuery* q_addUserOnClient;
	IQuery* q_addClientToken;
	IQuery* q_addUserToken;
	IQuery* q_addUserTokenWithGroup;
	IQuery* q_hasRecentFullOrIncrFileBackup;
	IQuery* q_hasRecentIncrFileBackup;
	IQuery* q_hasRecentFullOrIncrImageBackup;
	IQuery* q_hasRecentIncrImageBackup;
	IQuery* q_addRestore;
	IQuery* q_getRestorePath;
	IQuery* q_getRestoreIdentity;
	IQuery* q_setRestoreDone;
	IQuery* q_deleteRestore;
	IQuery* q_getFileBackupInfo;
	IQuery* q_setVirtualMainClient;
	IQuery* q_deleteUsedAccessTokens;
	IQuery* q_hasUsedAccessToken;
	IQuery* q_addUsedAccessToken;
	IQuery* q_getClientnameByImageid;
	IQuery* q_getClientidByImageid;
	IQuery* q_getImageMounttime;
	IQuery* q_updateImageMounted;
	IQuery* q_addImageMounted;
	IQuery* q_delImageMounted;
	IQuery* q_getMountedImage;
	IQuery* q_getImageInfo;
	IQuery* q_getOldMountedImages;
	IQuery* q_setCapa;
	IQuery* q_getCapa;
	IQuery* q_getClientWithHashes;
	IQuery* q_updateClientWithHashes;
	//@-SQLGenVariablesEnd

	IDatabase *db;
};
