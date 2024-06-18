#include "action_header.h"
#include "../server_settings.h"
#include "../ClientMain.h"
#include "../snapshot_helper.h"
#include "../server.h"

#ifndef NAME_MAX
#define NAME_MAX _POSIX_NAME_MAX
#endif

namespace
{
	void access_dir_details(std::string folder, std::string& ret)
	{
		bool has_error = false;
		getFiles(folder, &has_error);
		if (has_error)
		{
			ret += "Cannot access " + folder + ". " + os_last_error_str() + "\n";
		}
		else
		{
			ret += "Can access " + folder + "\n";
		}
	}

	std::string access_err_details(std::string folder)
	{
		std::vector<std::string> toks;
		Tokenize(folder, toks, os_file_sep());

		std::string ret;

		std::string cdir = os_file_sep();
		access_dir_details(cdir, ret);

		for (size_t i = 0; i < toks.size(); ++i)
		{
			if (toks[i].empty()) continue;

			if (cdir != os_file_sep())
			{
				cdir += os_file_sep();
			}
			cdir += toks[i];

			access_dir_details(cdir, ret);
		}

		return ret;
	}

	std::string access_dir_hint(std::string folder)
	{
		if (folder.size() > 1 && folder[1] == ':')
		{
			bool has_error = false;
			getFiles(folder.substr(0, 2) + os_file_sep(), &has_error);

			if (has_error)
			{
				return "volume_not_accessible";
			}
		}

		if ((folder.size() > 2 && folder[0] == '\\'
			&& folder[1] == '\\'
			&& folder[2] != '?')
			|| next(folder, 0, "\\\\?\\UNC"))
		{
			bool has_error = false;
			getFiles(folder, &has_error);

			if (has_error && os_last_error() == 5
				|| os_last_error() == 1326)
			{
				return "folder_unc_access_denied";
			}
		}

		return std::string();
	}

	bool is_stop_show(IDatabase* db, std::string stop_key)
	{
		db_results res = db->Read("SELECT tvalue FROM misc WHERE tkey='stop_show'");
		if (!res.empty())
		{
			std::vector<std::string> toks;
			Tokenize(res[0]["tvalue"], toks, ",");
			return std::find(toks.begin(), toks.end(), stop_key) != toks.end();
		}
		return false;
	}


	void add_stop_show(IDatabase* db, JSON::Object& ret, std::string dir_error_stop_show_key)
	{
		ret.set("dir_error_stop_show_key", dir_error_stop_show_key);
		if (is_stop_show(db, dir_error_stop_show_key))
		{
			ret.set("dir_error_show", false);
		}
	}

	void access_dir_checks(IDatabase* db, ServerSettings& settings, std::string backupfolder, std::string backupfolder_uncompr,
		JSON::Object& ret)
	{
#ifdef _WIN32
		if (backupfolder.size() == 2 && backupfolder[1] == ':')
		{
			backupfolder += os_file_sep();
		}
		if (backupfolder_uncompr.size() == 2 && backupfolder_uncompr[1] == ':')
		{
			backupfolder_uncompr += os_file_sep();
		}
#endif

		if (backupfolder.empty() || !os_directory_exists(os_file_prefix(backupfolder)) || !os_directory_exists(os_file_prefix(backupfolder_uncompr)))
		{
			if (!backupfolder.empty())
			{
				ret.set("system_err", os_last_error_str());
			}

			ret.set("dir_error", true);

			if (settings.getSettings()->backupfolder.empty())
			{
				ret.set("dir_error_ext", "err_name_empty");
			}
			else if (!os_directory_exists(os_file_prefix(settings.getSettings()->backupfolder)))
			{
				ret.set("dir_error_ext", "err_folder_not_found");
				add_stop_show(db, ret, "dir_error_not_found");
			}
			else
			{
				add_stop_show(db, ret, "dir_error_misc");
			}

#ifdef _WIN32
			std::string hint = access_dir_hint(settings.getSettings()->backupfolder);
			if (!hint.empty())
			{
				ret.set("dir_error_hint", hint);
			}
#endif

#ifndef _WIN32
			ret.set("detail_err_str", access_err_details(settings.getSettings()->backupfolder));
#endif
		}
		else if (!os_directory_exists(os_file_prefix(backupfolder + os_file_sep() + "clients")) && !os_create_dir(os_file_prefix(backupfolder + os_file_sep() + "clients")))
		{
			ret.set("system_err", os_last_error_str());
			ret.set("dir_error", true);
			ret.set("dir_error_ext", "err_cannot_create_subdir");

			add_stop_show(db, ret, "dir_error_cannot_create_subdir");

#ifdef _WIN32
			std::string hint = access_dir_hint(backupfolder);
			if (!hint.empty())
			{
				ret.set("dir_error_hint", hint);
			}
#endif
		}
		else if (BackupServer::getSnapshotMethod(false) == BackupServer::ESnapshotMethod_Btrfs
			&& SnapshotHelper::getBackupfolder() != trim(backupfolder))
		{
			ret.set("dir_error", true);
			ret.set("dir_error_ext", "err_btrfs_backupfolder_differs");

			add_stop_show(db, ret, "dir_error_btrfs_backupfolder_differs");
			ret.set("dir_error_hint", "err_btrfs_backupfolder_differs");
			ret.set("btrfs_backupfolder", SnapshotHelper::getBackupfolder());
			ret.set("urbackup_backupfolder", backupfolder);
		}
		else
		{
			bool has_access_error = false;
			std::string testfoldername = "testfolderHvfgh---dFFoeRRRf";
			std::string testfolderpath = backupfolder + os_file_sep() + testfoldername;
			if (os_directory_exists(os_file_prefix(testfolderpath)))
			{
				if (!os_remove_dir(os_file_prefix(testfolderpath)))
				{
					ret.set("system_err", os_last_error_str());
					ret.set("dir_error", true);
					ret.set("dir_error_ext", "err_cannot_create_subdir");
					add_stop_show(db, ret, "dir_error_cannot_create_subdir2");
					has_access_error = true;
#ifdef _WIN32
					std::string hint = access_dir_hint(backupfolder);
					if (!hint.empty())
					{
						ret.set("dir_error_hint", hint);
					}
#endif
				}
			}

			SSettings* server_settings = settings.getSettings();

			if (!has_access_error
				&& !os_create_dir(os_file_prefix(testfolderpath)))
			{
				ret.set("system_err", os_last_error_str());
				ret.set("dir_error", true);
				ret.set("dir_error_ext", "err_cannot_create_subdir");
				add_stop_show(db, ret, "dir_error_cannot_create_subdir3");

#ifdef _WIN32
				std::string hint = access_dir_hint(backupfolder);
				if (!hint.empty())
				{
					ret.set("dir_error_hint", hint);
				}
#endif
				has_access_error = true;
			}
			else if (!server_settings->no_file_backups
				&& os_directory_exists(os_file_prefix(backupfolder + os_file_sep() + "testfo~1")))
			{
				ret.set("dir_error", true);
				ret.set("dir_error_ext", "dos_names_created");
				add_stop_show(db, ret, "dos_names_created");
				ret.set("dir_error_hint", "dos_names_created");
				if (backupfolder.size() > 2
					&& backupfolder[1] == ':')
				{
					ret.set("dir_error_volume", backupfolder.substr(0, 2));
				}
				else
				{
					ret.set("dir_error_volume", "<VOLUME>");
				}
			}

			if (!server_settings->no_file_backups)
			{
				std::string linkfolderpath = testfolderpath + "_link";
				os_remove_symlink_dir(os_file_prefix(linkfolderpath));
				Server->deleteFile(os_file_prefix(linkfolderpath));

				if (!has_access_error
					&& !os_link_symbolic(os_file_prefix(testfolderpath), os_file_prefix(linkfolderpath)))
				{
					ret.set("system_err", os_last_error_str());
					ret.set("dir_error", true);
					ret.set("dir_error_ext", "err_cannot_create_symbolic_links");
					ret.set("dir_error_hint", "UrBackup cannot create symbolic links on the backup storage. "
						"Your backup storage must support symbolic links in order for UrBackup to work correctly. "
						"The UrBackup Server must run as administrative user on Windows (If not you get error code 1314). "
						"Note: As of 2016-05-07 samba (which is used by many Linux based NAS operating systems for Windows file sharing) has not "
						"implemented the necessary functionality to support symbolic link creation from Windows (With this you get error code 4390).");
					add_stop_show(db, ret, "dir_error_cannot_create_symbolic_links");
				}

				os_remove_symlink_dir(os_file_prefix(linkfolderpath));
			}

#ifndef _WIN32
			if (!server_settings->no_file_backups)
			{
				std::string test1_path = testfolderpath + os_file_sep() + "test1";
				std::string test2_path = testfolderpath + os_file_sep() + "Test1";

				if (!os_create_dir(test1_path)
					|| !os_create_dir(test2_path))
				{
					ret.set("system_err", os_last_error_str());
					ret.set("dir_error", true);
					ret.set("dir_error_ext", "err_file_system_case_insensitive");
					ret.set("dir_error_hint", "err_file_system_case_insensitive");
					add_stop_show(db, ret, "err_file_system_case_insensitive");
				}

				os_remove_dir(test1_path);
				os_remove_dir(test2_path);

				std::string test3_path = testfolderpath + os_file_sep() + "CON";

				std::unique_ptr<IFile> test_file(Server->openFile(test3_path, MODE_WRITE));

				if (test_file.get() == NULL)
				{
					ret.set("system_err", os_last_error_str());
					ret.set("dir_error", true);
					ret.set("dir_error_ext", "err_file_system_special_windows_files_disallowed");
					ret.set("dir_error_hint", "err_file_system_special_windows_files_disallowed");
					add_stop_show(db, ret, "err_file_system_special_windows_files_disallowed");
				}

				test_file.reset();
				Server->deleteFile(test3_path);
			}
#endif

			if (!server_settings->no_file_backups)
			{			
				std::string test1_path = testfolderpath + os_file_sep();
				std::string max_path_str;
#ifdef _WIN32
				std::string unicode_name = "\xC3\x84";
				for (size_t i = 0; i < 240; ++i)
				{
					test1_path += unicode_name;
				}
				max_path_str = "(255 UCS-2 code points/UTF-16 with max 2*255 bytes)";
#else
				std::string name_max_path(NAME_MAX - 1, 'a');
				test1_path += name_max_path;
				max_path_str = std::string("(max ") +convert(NAME_MAX) + " bytes)";
#endif

				if (!os_create_dir(os_file_prefix(test1_path)))
				{
					ret.set("system_err", os_last_error_str());
					ret.set("dir_error", true);
					ret.set("dir_error_ext", "err_long_create_failed");
					ret.set("dir_error_hint", "err_long_create_failed");
					ret.set("max_path_str", max_path_str);
					add_stop_show(db, ret, "err_long_create_failed");
				}

				os_remove_dir(os_file_prefix(test1_path));
			}

			if (!server_settings->no_file_backups &&
				!is_stop_show(db, "virus_error"))
			{
				bool use_tmpfiles = server_settings->use_tmpfiles;
				std::string tmpfile_path;
				if (!use_tmpfiles)
				{
					tmpfile_path = server_settings->backupfolder + os_file_sep() + "urbackup_tmp_files";
				}

				std::unique_ptr<IFile> tmp_f(ClientMain::getTemporaryFileRetry(use_tmpfiles, tmpfile_path, logid_t()));

				if (tmp_f.get() == NULL)
				{
					ret.set("tmpdir_error", true);
					ret.set("tmpdir_error_stop_show_key", "tmpdir_error");
					if (is_stop_show(db, "tmpdir_error"))
					{
						ret.set("tmpdir_error_show", false);
					}
				}
				else
				{
					std::string teststring = base64_decode("WDVPIVAlQEFQWzRcUFpYNTQoUF4pN0NDKTd9JEVJQ0FSLVNUQU5EQVJELUFOVElWSVJVUy1URVNULUZJTEUhJEgrSCo=");
					tmp_f->Write(teststring);
					std::string tmp_fn = tmp_f->getFilename();
					tmp_f.reset();
					tmp_f.reset(Server->openFile(tmp_fn, MODE_RW));

					std::string readstring;
					if (tmp_f.get() != NULL)
					{
						readstring = tmp_f->Read(static_cast<_u32>(teststring.size()));
					}

					tmp_f.reset();
					Server->deleteFile(tmp_fn);

					if (teststring != readstring)
					{
						ret.set("virus_error", true);
						ret.set("virus_error_path", ExtractFilePath(tmp_fn));
						ret.set("virus_error_stop_show_key", "virus_error");
						if (is_stop_show(db, "virus_error"))
						{
							ret.set("virus_error_show", false);
						}
					}
				}
			}

			if (!has_access_error
				&& !os_remove_dir(os_file_prefix(testfolderpath)))
			{
				ret.set("system_err", os_last_error_str());
				ret.set("dir_error", true);
				ret.set("dir_error_ext", "err_cannot_create_subdir");
				add_stop_show(db, ret, "dir_error_cannot_create_subdir4");
				has_access_error = true;
			}
		}

		IFile *tmp = Server->openTemporaryFile();
		ScopedDeleteFile delete_tmp_file(tmp);
		if (tmp == NULL)
		{
			ret.set("tmpdir_error", true);
			ret.set("tmpdir_error_stop_show_key", "tmpdir_error");
			if (is_stop_show(db, "tmpdir_error"))
			{
				ret.set("tmpdir_error_show", false);
			}
		}
	}
}

ACTION_IMPL(status_check)
{
	Helper helper(tid, &POST, &PARAMS);
	JSON::Object ret;

	std::string rights = helper.getRights("status");
	SUser *session = helper.getSession();
	if (session != NULL && session->id == SESSION_ID_INVALID) return;
	if (session != NULL && rights == "all")
	{
		IDatabase *db = helper.getDatabase();
		helper.releaseAll();
		ServerSettings settings(db);
		access_dir_checks(db, settings, settings.getSettings()->backupfolder,
			settings.getSettings()->backupfolder_uncompr, ret);
	}
	else
	{
		ret.set("error", 1);
	}
	helper.Write(ret.stringify(false));
}