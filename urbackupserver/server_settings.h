#ifndef SERVER_SETTINS_H
#define SERVER_SETTINS_H

#include "../Interface/SettingsReader.h"
#include "../Interface/Database.h"
#include "../Interface/Mutex.h"
#include <memory>

namespace
{
	const char* image_file_format_default = "default";
	const char* image_file_format_vhd = "vhd";
	const char* image_file_format_vhdz = "vhdz";
	const char* image_file_format_cowraw = "cowraw";
	const char* image_file_format_vhdx = "vhdx";
	const char* image_file_format_vhdxz = "vhdxz";

	const char* full_image_style_full = "full";
	const char* full_image_style_synthetic = "synthetic";

	const char* incr_image_style_to_full = "to-full";
	const char* incr_image_style_to_last = "to-last";

	const int c_use_undefined = -1;
	const int c_use_group = 1;
	const int c_use_value = 2;
	const char* c_use_value_str = "2";
	const int c_use_value_client = 4;
}

struct SSettings
{
	SSettings()
		: needs_update(false), refcount(0)
	{}

	volatile bool needs_update;
	size_t refcount;

	int clientid;
	std::string backupfolder;
	std::string backupfolder_uncompr;
	std::string update_freq_incr;
	std::string update_freq_full;
	std::string update_freq_image_full;
	std::string update_freq_image_incr;
	int max_file_incr;
	int min_file_incr;
	int max_file_full;
	int min_file_full;
	int min_image_incr;
	int max_image_incr;
	int min_image_full;
	int max_image_full;
	bool no_images;
	bool no_file_backups;
	bool allow_overwrite;
	bool autoshutdown;
	int startup_backup_delay;
	bool download_client;
	bool autoupdate_clients;
	int max_sim_backups;
	int max_active_clients;
	std::string backup_window_incr_file;
	std::string backup_window_full_file;
	std::string backup_window_incr_image;
	std::string backup_window_full_image;
	std::string computername;
	std::string virtual_clients;
	std::string virtual_clients_add;
	std::string exclude_files;
	std::string include_files;
	std::string default_dirs;
	bool backup_dirs_optional;
	std::string cleanup_window;
	bool allow_pause;
	bool allow_starting_full_file_backups;
	bool allow_starting_incr_file_backups;
	bool allow_starting_full_image_backups;
	bool allow_starting_incr_image_backups;
	bool allow_config_paths;
	bool allow_log_view;
	bool allow_tray_exit;
	std::string image_letters;
	bool backup_database;
	std::string internet_server;
	bool client_set_settings;
	unsigned short internet_server_port;
	std::string internet_server_proxy;
	std::string internet_authkey;
	bool internet_full_file_backups;
	bool internet_image_backups;
	bool internet_encrypt;
	bool internet_compress;
	int internet_compression_level;
	std::string local_speed;
	std::string internet_speed;
	std::string global_internet_speed;
	std::string global_local_speed;
	bool internet_mode_enabled;
	bool silent_update;
	bool use_tmpfiles;
	bool use_tmpfiles_images;
	std::string tmpdir;
	std::string local_full_file_transfer_mode;
	std::string internet_full_file_transfer_mode;
	std::string local_incr_file_transfer_mode;
	std::string internet_incr_file_transfer_mode;
	std::string local_image_transfer_mode;
	std::string internet_image_transfer_mode;
	size_t update_stats_cachesize;
	std::string global_soft_fs_quota;
	std::string client_quota;
	bool end_to_end_file_backup_verification;
	bool internet_calculate_filehashes_on_client;
	bool internet_parallel_file_hashing;
	bool use_incremental_symlinks;
	std::string image_file_format;
	bool internet_connect_always;
	bool show_server_updates;
	std::string server_url;
	bool verify_using_client_hashes;
	bool internet_readd_file_entries;
	std::string client_access_key;
	int max_running_jobs_per_client;
	bool background_backups;
	bool create_linked_user_views;
	std::string local_incr_image_style;
	std::string local_full_image_style;
	std::string internet_incr_image_style;
	std::string internet_full_image_style;
	float backup_ok_mod_file;
	float backup_ok_mod_image;
	std::string cbt_volumes;
	std::string cbt_crash_persistent_volumes;
	bool ignore_disk_errors;
	std::string vss_select_components;
	bool allow_file_restore;
	bool allow_component_restore;
	bool allow_component_config;
	std::string image_snapshot_groups;
	std::string file_snapshot_groups;
	int64 internet_file_dataplan_limit;
	int64 internet_image_dataplan_limit;
	int alert_script;
	std::string alert_params;
	std::string archive;
	std::string client_settings_tray_access_pw;
	bool local_encrypt;
	bool local_compress;
	int download_threads;
	int hash_threads;
	int client_hash_threads;
	int image_compress_threads;
	std::string ransomware_canary_paths;
	std::string backup_dest_url;
	std::string backup_dest_params;
	std::string backup_dest_secret_params;
	bool pause_if_windows_unlocked;
	std::string backup_unlocked_window;
};

struct SLDAPSettings
{
	SLDAPSettings()
		: server_port(0)
	{

	}

	bool login_enabled;
	std::string server_name;
	int server_port;
	std::string username_prefix;
	std::string username_suffix;
	std::string group_class_query;
	std::string group_key_name;
	std::string class_key_name;
	std::map<std::string, std::string> group_rights_map;
	std::map<std::string, std::string> class_rights_map;
};

struct STimeSpan
{
	STimeSpan(void): dayofweek(-1), numdays(7) {}
	STimeSpan(int dayofweek, float start_hour, float stop_hour):dayofweek(dayofweek), start_hour(start_hour), stop_hour(stop_hour), numdays(1) {}
	STimeSpan(float start_hour, float stop_hour):dayofweek(0), start_hour(start_hour), stop_hour(stop_hour), numdays(1) {}

	int dayofweek;
	int numdays;
	float start_hour;
	float stop_hour;

	float duration()
	{
		if(dayofweek==-1)
		{
			return 24.f*numdays;
		}
		else
		{
			if (stop_hour < start_hour)
			{
				return (stop_hour + 24 - start_hour)*numdays;
			}
			else
			{
				return (stop_hour - start_hour)*numdays;
			}
		}
	}
};

class ServerSettings
{
public:
	ServerSettings(IDatabase *db, int pClientid=0);
	~ServerSettings(void);

	void update(bool force_update);

	SSettings *getSettings(bool *was_updated=NULL);

	static void init_mutex(void);
	static void destroy_mutex(void);
	static void clear_cache();
	static void updateAll();
	static void updateClient(int clientid);
	static std::string generateRandomAuthKey(size_t len=10);
	static std::string generateRandomBinaryKey(void);

	std::vector<STimeSpan> getBackupWindowIncrFile(void);
	std::vector<STimeSpan> getBackupWindowFullFile(void);
	std::vector<STimeSpan> getBackupWindowIncrImage(void);
	std::vector<STimeSpan> getBackupWindowFullImage(void);

	std::vector<STimeSpan> getCleanupWindow(void);
	std::vector<std::string> getBackupVolumes(const std::string& all_volumes, const std::string& all_nonusb_volumes);

	std::string getImageFileFormat();

	int getUpdateFreqImageIncr();
	int getUpdateFreqFileIncr();
	int getUpdateFreqImageFull();
	int getUpdateFreqFileFull();

	int getLocalSpeed();
	int getGlobalLocalSpeed();
	int getInternetSpeed();
	int getGlobalInternetSpeed();

	std::string getVirtualClients();

	static bool isInTimeSpan(std::vector<STimeSpan> bw);

	SLDAPSettings getLDAPSettings();

	std::string ldapMapToString(const std::map<std::string, std::string>& ldap_map);

	static void createSettingsReaders(IDatabase* db, int clientid, std::unique_ptr<ISettingsReader>& settings_default,
		std::unique_ptr<ISettingsReader>& settings_client, std::unique_ptr<ISettingsReader>& settings_global, int& settings_default_id);

	std::string getImageFileFormatInt(const std::string& image_file_format);

	static void readStringClientSetting(IDatabase* db, int clientid, const std::string &name, const std::string& merge_sep, std::string *output, bool allow_client_value = true);

private:
	void operator=(const ServerSettings& other){};
	ServerSettings(const ServerSettings& other){};

	std::vector<STimeSpan> getWindow(std::string window);

	std::vector<STimeSpan> parseTimeSpan(std::string time_span);

	std::vector<std::pair<double, STimeSpan > > parseTimeSpanValue(std::string time_span_value);

	double currentTimeSpanValue(std::string time_span_value);

	void readSettings();


	float parseTimeDet(std::string t);
	STimeSpan parseTime(std::string t);
	int parseDayOfWeek(std::string dow);
	void readSettingsDefault(ISettingsReader* settings_default, ISettingsReader* settings_global, IQuery* q_get_client_setting);
	void readSettingsClient(ISettingsReader* settings_client, IQuery* q_get_client_setting);

	void readStringClientSetting(IQuery* q_get_client_setting, const std::string &name, const std::string& merge_sep, std::string *output, bool allow_client_value=true);
	static void readStringClientSetting(IQuery* q_get_client_setting, int clientid, const std::string &name, const std::string& merge_sep, std::string *output, bool allow_client_value = true);
	std::string readValClientSetting(IQuery* q_get_client_setting, const std::string &name, bool allow_client_value);
	void readBoolClientSetting(IQuery* q_get_client_setting, const std::string &name, bool *output, bool allow_client_value = true);
	void readIntClientSetting(IQuery* q_get_client_setting, const std::string &name, int *output, bool allow_client_value = true);
	void readInt64ClientSetting(IQuery* q_get_client_setting, const std::string &name, int64 *output, bool allow_client_value = true);
	void readSizeClientSetting(IQuery* q_get_client_setting, const std::string &name, size_t *output, bool allow_client_value = true);

	void updateInternal(bool* was_updated);
	std::map<std::string, std::string> parseLdapMap(const std::string& data);

	SSettings* local_settings;

	IDatabase* db;

	int clientid;

	static std::map<int, SSettings*> g_settings_cache;
	static IMutex *g_mutex;
};


#endif //SERVER_SETTINS_H
