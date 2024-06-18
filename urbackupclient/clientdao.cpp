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

#include "clientdao.h"
#include "../stringtools.h"
#include "../Interface/Server.h"
#include <memory.h>

const int ClientDAO::c_is_group = 0;
const int ClientDAO::c_is_user = 1;
const int ClientDAO::c_is_system_user = 2;

ClientDAO::ClientDAO(IDatabase *pDB)
{
	db=pDB;
	prepareQueries();
}

ClientDAO::~ClientDAO()
{
	destroyQueries();
}

void ClientDAO::prepareQueries()
{
	q_get_files=db->Prepare("SELECT data, num, generation FROM files WHERE name=? AND tgroup=?", false);
	q_add_files=db->Prepare("INSERT OR REPLACE INTO files (name, tgroup, num, data, generation) VALUES (?,?,?,?,?)", false);
	q_get_dirs=db->Prepare("SELECT name, path, id, optional, tgroup, symlinked, server_default, reset_keep, facet FROM backupdirs ORDER BY id ASC", false);
	q_remove_all=db->Prepare("DELETE FROM files", false);
	q_get_changed_dirs=db->Prepare("SELECT id, name FROM mdirs WHERE name GLOB ? UNION SELECT id, name FROM mdirs_backup WHERE name GLOB ?", false);
	q_modify_files=db->Prepare("UPDATE files SET data=?, num=?, generation=? WHERE name=? AND tgroup=? AND generation=?", false);
	q_has_files=db->Prepare("SELECT count(*) AS num FROM files WHERE name=? AND tgroup=?", false);
	q_insert_shadowcopy=db->Prepare("INSERT INTO shadowcopies (vssid, ssetid, target, path, tname, orig_target, filesrv, vol, starttime, refs, starttoken, clientsubname) VALUES (?, ?, ?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP, ?, ?, ?)", false);
	q_get_shadowcopies=db->Prepare("SELECT id, vssid, ssetid, target, path, tname, orig_target, filesrv, vol, (strftime('%s','now') - strftime('%s', starttime)) AS passedtime, refs, starttoken, clientsubname FROM shadowcopies", false);
	q_remove_shadowcopies=db->Prepare("DELETE FROM shadowcopies WHERE id=?", false);
	q_save_changed_dirs=db->Prepare("INSERT OR REPLACE INTO mdirs_backup SELECT id, name FROM mdirs WHERE name GLOB ?", false);
	q_delete_saved_changed_dirs=db->Prepare("DELETE FROM mdirs_backup", false);
	q_has_changed_gap=db->Prepare("SELECT name FROM mdirs WHERE name GLOB '##-GAP-##*'", false);
	q_get_del_dirs=db->Prepare("SELECT name FROM del_dirs WHERE name GLOB ? UNION SELECT name FROM del_dirs_backup WHERE name GLOB ?", false);
	q_del_del_dirs=db->Prepare("DELETE FROM del_dirs WHERE name GLOB ?", false);
	q_copy_del_dirs=db->Prepare("INSERT INTO del_dirs_backup SELECT name FROM del_dirs WHERE name GLOB ?", false);
	q_del_del_dirs_copy=db->Prepare("DELETE FROM del_dirs_backup", false);
	q_remove_del_dir=db->Prepare("DELETE FROM files WHERE name GLOB ? AND tgroup=?", false);
	q_get_shadowcopy_refcount=db->Prepare("SELECT refs FROM shadowcopies WHERE id=?", false);
	q_set_shadowcopy_refcount=db->Prepare("UPDATE shadowcopies SET refs=? WHERE id=?", false);
	q_get_pattern=db->Prepare("SELECT tvalue FROM misc WHERE tkey=?", false);
	q_insert_pattern=db->Prepare("INSERT INTO misc (tkey, tvalue) VALUES (?, ?)", false);
	q_update_pattern=db->Prepare("UPDATE misc SET tvalue=? WHERE tkey=?", false);
	prepareQueriesGen();
}

void ClientDAO::destroyQueries(void)
{
	db->destroyQuery(q_get_files);
	db->destroyQuery(q_add_files);
	db->destroyQuery(q_get_dirs);
	db->destroyQuery(q_remove_all);
	db->destroyQuery(q_get_changed_dirs);
	db->destroyQuery(q_modify_files);
	db->destroyQuery(q_has_files);
	db->destroyQuery(q_insert_shadowcopy);
	db->destroyQuery(q_get_shadowcopies);
	db->destroyQuery(q_remove_shadowcopies);
	db->destroyQuery(q_save_changed_dirs);
	db->destroyQuery(q_delete_saved_changed_dirs);
	db->destroyQuery(q_has_changed_gap);
	db->destroyQuery(q_get_del_dirs);
	db->destroyQuery(q_del_del_dirs);
	db->destroyQuery(q_copy_del_dirs);
	db->destroyQuery(q_del_del_dirs_copy);
	db->destroyQuery(q_remove_del_dir);
	db->destroyQuery(q_get_shadowcopy_refcount);
	db->destroyQuery(q_set_shadowcopy_refcount);
	db->destroyQuery(q_get_pattern);
	db->destroyQuery(q_insert_pattern);
	db->destroyQuery(q_update_pattern);
	destroyQueriesGen();
}

//@-SQLGenSetup
void ClientDAO::prepareQueriesGen(void)
{
	q_updateShadowCopyStarttime=nullptr;
	q_updateFileAccessToken=nullptr;
	q_getFileAccessTokens=nullptr;
	q_getFileAccessTokenId2Alts=nullptr;
	q_getFileAccessTokenId=nullptr;
	q_removeGroupMembership=nullptr;
	q_updateGroupMembership=nullptr;
	q_getGroupMembership=nullptr;
	q_addBackupDir=nullptr;
	q_delBackupDir=nullptr;
	q_setResetKeep=nullptr;
	q_resetHardlink=nullptr;
	q_hasHardLink=nullptr;
	q_addHardlink=nullptr;
	q_resetAllHardlinks=nullptr;
	q_getClientFacet=nullptr;
	q_getClientFacetByName=nullptr;
	q_addClientFacet=nullptr;
	q_updateClientFacet=nullptr;
}

//@-SQLGenDestruction
void ClientDAO::destroyQueriesGen(void)
{
	db->destroyQuery(q_updateShadowCopyStarttime);
	db->destroyQuery(q_updateFileAccessToken);
	db->destroyQuery(q_getFileAccessTokens);
	db->destroyQuery(q_getFileAccessTokenId2Alts);
	db->destroyQuery(q_getFileAccessTokenId);
	db->destroyQuery(q_removeGroupMembership);
	db->destroyQuery(q_updateGroupMembership);
	db->destroyQuery(q_getGroupMembership);
	db->destroyQuery(q_addBackupDir);
	db->destroyQuery(q_delBackupDir);
	db->destroyQuery(q_setResetKeep);
	db->destroyQuery(q_resetHardlink);
	db->destroyQuery(q_hasHardLink);
	db->destroyQuery(q_addHardlink);
	db->destroyQuery(q_resetAllHardlinks);
	db->destroyQuery(q_getClientFacet);
	db->destroyQuery(q_getClientFacetByName);
	db->destroyQuery(q_addClientFacet);
	db->destroyQuery(q_updateClientFacet);
}

void ClientDAO::restartQueries(void)
{
	destroyQueries();
	prepareQueries();
}

std::string ClientDAO::escapeGlob(const std::string& glob)
{
	std::string ret;
	ret.reserve(glob.size());
	for (size_t i = 0; i<glob.size(); ++i)
	{
		if (glob[i] == '?')
		{
			ret += "[?]";
		}
		else if (glob[i] == '[')
		{
			ret += "[[]";
		}
		else if (glob[i] == '*')
		{
			ret += "[*]";
		}
		else
		{
			ret += glob[i];
		}
	}
	return ret;
}

bool ClientDAO::getFiles(std::string path, int tgroup, std::vector<SFileAndHash> &data, int64& generation)
{
	q_get_files->Bind(path);
	q_get_files->Bind(tgroup);
	db_results res=q_get_files->Read();
	q_get_files->Reset();
	if(res.size()==0)
		return false;

	generation = watoi64(res[0]["generation"]);

	std::string &qdata=res[0]["data"];

	if(qdata.empty())
		return true;

	int num=watoi(res[0]["num"]);
	char *ptr=(char*)&qdata[0];
	while(ptr-(char*)&qdata[0]<num)
	{
		SFileAndHash f;
		unsigned short ss;
		memcpy(&ss, ptr, sizeof(unsigned short));
		ptr+=sizeof(unsigned short);
		std::string tmp;
		tmp.resize(ss);
		memcpy(&tmp[0], ptr, ss);
		f.name=(tmp);
		ptr+=ss;
		memcpy(&f.size, ptr, sizeof(int64));
		ptr+=sizeof(int64);
		memcpy(&f.change_indicator, ptr, sizeof(uint64));
		ptr+=sizeof(uint64);
		char isdir=*ptr;
		++ptr;
		if(isdir==0)
			f.isdir=false;
		else
			f.isdir=true;
		
		unsigned short hashsize;
		memcpy(&hashsize, ptr, sizeof(unsigned short));
		ptr+=sizeof(unsigned short);

		f.hash.resize(hashsize);
		if(hashsize>0)
		{
			memcpy(&f.hash[0], ptr, hashsize);
		}

		ptr+=hashsize;

		char issym=*ptr;
		++ptr;
		f.issym=issym==0?false:true;

		char isspecialf=*ptr;
		++ptr;
		f.isspecialf= isspecialf ==0?false:true;


		if(f.issym)
		{
			memcpy(&ss, ptr, sizeof(unsigned short));
			ptr+=sizeof(unsigned short);
			if(ss>0)
			{
				tmp.resize(ss);
				memcpy(&tmp[0], ptr, ss);
				f.symlink_target=(tmp);
				ptr+=ss;
			}			
		}

		data.push_back(f);
	}
	return true;
}

char * constructData(const std::vector<SFileAndHash> &data, size_t &datasize)
{
	datasize=0;
	std::vector<std::string> utf;
	utf.resize(data.size());
	for(size_t i=0;i<data.size();++i)
	{
		utf[i]=(data[i].name);
		datasize+=utf[i].size();
		datasize+=sizeof(unsigned short);
		datasize+=sizeof(int64)*2;
		++datasize; //isdir
		datasize+=sizeof(unsigned short);
		datasize+=data[i].hash.size();
		++datasize; //issym
		++datasize; //isspecial
		if(data[i].issym)
		{
			datasize+=sizeof(unsigned short);
			datasize+=(data[i].symlink_target).size();
		}
	}
	char *buffer=new char[datasize];
	char *ptr=buffer;
	for(size_t i=0;i<data.size();++i)
	{
		unsigned short ss=(unsigned short)utf[i].size();
		memcpy(ptr, (char*)&ss, sizeof(unsigned short));
		ptr+=sizeof(unsigned short);
		memcpy(ptr, &utf[i][0], ss);
		ptr+=ss;
		memcpy(ptr, (char*)&data[i].size, sizeof(int64));
		ptr+=sizeof(int64);
		memcpy(ptr, (char*)&data[i].change_indicator, sizeof(uint64));
		ptr+=sizeof(uint64);
		char isdir=1;
		if(!data[i].isdir)
			isdir=0;
		*ptr=isdir;
		ptr+=sizeof(char);
		unsigned short hashsize=static_cast<unsigned short>(data[i].hash.size());
		memcpy(ptr, &hashsize, sizeof(hashsize));
		ptr+=sizeof(hashsize);
		memcpy(ptr, data[i].hash.data(), hashsize);
		ptr+=hashsize;
		char issym = data[i].issym?1:0;
		*ptr=issym;
		++ptr;
		char isspecialf = data[i].isspecialf?1:0;
		*ptr= isspecialf ?1:0;
		++ptr;
		if(data[i].issym)
		{
			std::string symlink_target = (data[i].symlink_target);
			ss=(unsigned short)symlink_target.size();
			memcpy(ptr, (char*)&ss, sizeof(unsigned short));
			ptr+=sizeof(unsigned short);
			if(ss>0)
			{
				memcpy(ptr, &symlink_target[0], ss);
			}
			ptr+=ss;
		}
	}
	return buffer;
}

std::string guidToString( GUID guid )
{
	return bytesToHex(reinterpret_cast<unsigned char*>(&guid), sizeof(guid));
}

GUID randomGuid()
{
	GUID ret;
	Server->secureRandomFill(reinterpret_cast<char*>(&ret), sizeof(ret));
	return ret;
}

void ClientDAO::addFiles(std::string path, int tgroup, const std::vector<SFileAndHash> &data, int64 target_generation)
{
	size_t ds;
	char *buffer=constructData(data, ds);
	q_add_files->Bind(path);
	q_add_files->Bind(tgroup);
	q_add_files->Bind(ds);
	q_add_files->Bind(buffer, (_u32)ds);
	q_add_files->Bind(target_generation);
	q_add_files->Write();
	q_add_files->Reset();
	delete []buffer;
}

void ClientDAO::modifyFiles(std::string path, int tgroup, const std::vector<SFileAndHash> &data, int64 target_generation)
{
	size_t ds;
	char *buffer=constructData(data, ds);
	q_modify_files->Bind(buffer, (_u32)ds);
	q_modify_files->Bind(ds);
	q_modify_files->Bind(target_generation+1);
	q_modify_files->Bind(path);
	q_modify_files->Bind(tgroup);
	q_modify_files->Bind(target_generation);
	q_modify_files->Write();
	q_modify_files->Reset();
	delete []buffer;
}

bool ClientDAO::hasFiles(std::string path, int tgroup)
{
	q_has_files->Bind(path);
	q_has_files->Bind(tgroup);
	db_results res=q_has_files->Read();
	q_has_files->Reset();
	if(res.size()>0)
		return res[0]["num"]=="1";
	else
		return false;
}

std::vector<SBackupDir> ClientDAO::getBackupDirs(void)
{
	db_results res=q_get_dirs->Read();
	q_get_dirs->Reset();

	std::vector<SBackupDir> ret;
	std::vector<SBackupDir> sym_ret;
	for(size_t i=0;i<res.size();++i)
	{
		SBackupDir dir;
		dir.id=watoi(res[i]["id"]);
		dir.tname=res[i]["name"];
		dir.path=res[i]["path"];
		dir.flags=watoi(res[i]["optional"]);
		dir.group=watoi(res[i]["tgroup"]);
		int server_default = watoi(res[i]["server_default"]);
		if (server_default < 0 || server_default>2)
			dir.server_default = EBackupDirServerDefault_No;
		else
			dir.server_default = static_cast<EBackupDirServerDefault>(server_default);
		dir.symlinked=(res[i]["symlinked"]=="1");
		dir.symlinked_confirmed=false;
		dir.reset_keep = (res[i]["reset_keep"] == "1");
		dir.facet = watoi(res[i]["facet"]);

		if(dir.tname!="*")
		{
			if(dir.symlinked)
			{
				sym_ret.push_back(dir);
			}
			else
			{
				ret.push_back(dir);
			}			
		}
	}

	ret.insert(ret.end(), sym_ret.begin(), sym_ret.end());

	return ret;
}

void ClientDAO::removeAllFiles(void)
{
	q_remove_all->Write();
}

std::vector<std::string> ClientDAO::getChangedDirs(const std::string& path, bool backup)
{
	std::vector<std::string> ret;

	std::string sep = os_file_sep();
	if(path == "##-GAP-##")
	{
		sep = "";
	}

	q_get_changed_dirs->Bind(escapeGlob(path)+sep+"*");
	q_get_changed_dirs->Bind(escapeGlob(path)+sep+"*");
	db_results res=q_get_changed_dirs->Read();
	q_get_changed_dirs->Reset();

	ret.reserve(res.size());
	for(size_t i=0;i<res.size();++i)
	{
		ret.push_back(res[i]["name"] );
	}

	if (backup)
	{
		q_save_changed_dirs->Bind(escapeGlob(path) + sep + "*");
		q_save_changed_dirs->Write();
		q_save_changed_dirs->Reset();
	}

	return ret;
}

std::vector<SShadowCopy> ClientDAO::getShadowcopies(void)
{
	db_results res=q_get_shadowcopies->Read();
	q_get_shadowcopies->Reset();
	std::vector<SShadowCopy> ret;
	for(size_t i=0;i<res.size();++i)
	{
		db_single_result &r=res[i];
		SShadowCopy sc;
		sc.id=watoi(r["id"]);
		memcpy(&sc.vssid, r["vssid"].c_str(), sizeof(GUID) );
		memcpy(&sc.ssetid, r["ssetid"].c_str(), sizeof(GUID) );
		sc.target=r["target"];
		sc.path=r["path"];
		sc.tname=r["tname"];
		sc.orig_target=r["orig_target"];
		sc.filesrv=r["filesrv"]=="0"?false:true;
		sc.vol=r["vol"];
		sc.passedtime=watoi(r["passedtime"]);
		sc.refs=watoi(r["refs"]);
		sc.starttoken=r["starttoken"];
		sc.clientsubname = r["clientsubname"];
		ret.push_back(sc);
	}
	return ret;
}

int ClientDAO::addShadowcopy(const SShadowCopy &sc)
{
	DBScopedSynchronous sync_db(db);

	q_insert_shadowcopy->Bind((char*)&sc.vssid, sizeof(GUID) );
	q_insert_shadowcopy->Bind((char*)&sc.ssetid, sizeof(GUID) );
	q_insert_shadowcopy->Bind(sc.target);
	q_insert_shadowcopy->Bind(sc.path);
	q_insert_shadowcopy->Bind(sc.tname);
	q_insert_shadowcopy->Bind(sc.orig_target);
	q_insert_shadowcopy->Bind(sc.filesrv?1:0);
	q_insert_shadowcopy->Bind(sc.vol);
	q_insert_shadowcopy->Bind(sc.refs);
	q_insert_shadowcopy->Bind(sc.starttoken);
	q_insert_shadowcopy->Bind(sc.clientsubname);
	q_insert_shadowcopy->Write();
	q_insert_shadowcopy->Reset();
	return (int)db->getLastInsertID();
}

int ClientDAO::modShadowcopyRefCount(int id, int m)
{
	DBScopedSynchronous sync_db(db);

	q_get_shadowcopy_refcount->Bind(id);
	db_results res=q_get_shadowcopy_refcount->Read();
	q_get_shadowcopy_refcount->Reset();
	if(!res.empty())
	{
		int refs=watoi(res[0]["refs"]);
		refs+=m;
		q_set_shadowcopy_refcount->Bind(refs);
		q_set_shadowcopy_refcount->Bind(id);
		q_set_shadowcopy_refcount->Write();
		q_set_shadowcopy_refcount->Reset();
		return refs;
	}
	return -1;
}

void ClientDAO::deleteShadowcopy(int id)
{
	DBScopedSynchronous sync_db(db);

	q_remove_shadowcopies->Bind(id);
	q_remove_shadowcopies->Write();
	q_remove_shadowcopies->Reset();
}

void ClientDAO::deleteSavedChangedDirs(void)
{
	q_delete_saved_changed_dirs->Write();
	q_delete_saved_changed_dirs->Reset();
}

bool ClientDAO::hasChangedGap(void)
{
	db_results res=q_has_changed_gap->Read();
	q_has_changed_gap->Reset();
	return !res.empty();
}

std::vector<std::string> ClientDAO::getGapDirs(void)
{
	db_results res=q_has_changed_gap->Read();
	q_has_changed_gap->Reset();
	std::vector<std::string> ret;
	for(size_t i=0;i<res.size();++i)
	{
		std::string gap=res[i]["name"];
		gap.erase(0,9);
		ret.push_back(gap);
	}
	return ret;
}

std::vector<std::string> ClientDAO::getDelDirs(const std::string& path, bool del)
{
	std::vector<std::string> ret;

	if(del)
	{
		q_copy_del_dirs->Bind(escapeGlob(path)+os_file_sep()+"*");
		q_copy_del_dirs->Write();
		q_copy_del_dirs->Reset();
		q_del_del_dirs->Bind(escapeGlob(path)+os_file_sep()+"*");
		q_del_del_dirs->Write();
		q_del_del_dirs->Reset();
	}

	q_get_del_dirs->Bind(escapeGlob(path)+os_file_sep()+"*");
	q_get_del_dirs->Bind(escapeGlob(path)+os_file_sep()+"*");
	db_results res=q_get_del_dirs->Read();
	q_get_del_dirs->Reset();

	for(size_t i=0;i<res.size();++i)
	{
		ret.push_back(res[i]["name"]);
	}
	return ret;
}

void ClientDAO::deleteSavedDelDirs(void)
{
	q_del_del_dirs_copy->Write();
	q_del_del_dirs_copy->Reset();
}

void ClientDAO::removeDeletedDir(const std::string &dir, int tgroup)
{
	q_remove_del_dir->Bind(escapeGlob(dir)+"*");
	q_remove_del_dir->Bind(tgroup);
	q_remove_del_dir->Write();
	q_remove_del_dir->Reset();
}

const std::string exclude_pattern_key="exclude_pattern";

std::string ClientDAO::getOldExcludePattern(void)
{
	return getMiscValue(exclude_pattern_key);
}

void ClientDAO::updateOldExcludePattern(const std::string &pattern)
{
	updateMiscValue(exclude_pattern_key, pattern);
}

const std::string include_pattern_key="include_pattern";

std::string ClientDAO::getOldIncludePattern(void)
{
	return getMiscValue(include_pattern_key);
}

void ClientDAO::updateOldIncludePattern(const std::string &pattern)
{
	updateMiscValue(include_pattern_key, pattern);
}

std::string ClientDAO::getMiscValue(const std::string& key)
{
	q_get_pattern->Bind(key);
	db_results res=q_get_pattern->Read();
	q_get_pattern->Reset();
	if(!res.empty())
	{
		return res[0]["tvalue"];
	}
	else
	{
		return "";
	}
}


void ClientDAO::updateMiscValue(const std::string& key, const std::string& value)
{
	q_get_pattern->Bind(key);
	db_results res=q_get_pattern->Read();
	q_get_pattern->Reset();
	if(!res.empty())
	{
		q_update_pattern->Bind(value);
		q_update_pattern->Bind(key);
		q_update_pattern->Write();
		q_update_pattern->Reset();
	}
	else
	{
		q_insert_pattern->Bind(key);
		q_insert_pattern->Bind(value);
		q_insert_pattern->Write();
		q_insert_pattern->Reset();
	}
}

/**
* @-SQLGenAccess
* @func void ClientDAO::updateShadowCopyStarttime
* @sql
*    UPDATE shadowcopies SET starttime=CURRENT_TIMESTAMP
*           WHERE id=:id(int)
*/
void ClientDAO::updateShadowCopyStarttime(int id)
{
	if(q_updateShadowCopyStarttime==nullptr)
	{
		q_updateShadowCopyStarttime=db->Prepare("UPDATE shadowcopies SET starttime=CURRENT_TIMESTAMP WHERE id=?", false);
	}
	q_updateShadowCopyStarttime->Bind(id);
	q_updateShadowCopyStarttime->Write();
	q_updateShadowCopyStarttime->Reset();
}

/**
* @-SQLGenAccess
* @func void ClientDAO::updateFileAccessToken
* @sql
*    INSERT OR REPLACE INTO fileaccess_tokens
*          (accountname, token, is_user)
*      VALUES
*           (:accountname(string), :token(string), :is_user(int))
*/
void ClientDAO::updateFileAccessToken(const std::string& accountname, const std::string& token, int is_user)
{
	if(q_updateFileAccessToken==nullptr)
	{
		q_updateFileAccessToken=db->Prepare("INSERT OR REPLACE INTO fileaccess_tokens (accountname, token, is_user) VALUES (?, ?, ?)", false);
	}
	q_updateFileAccessToken->Bind(accountname);
	q_updateFileAccessToken->Bind(token);
	q_updateFileAccessToken->Bind(is_user);
	q_updateFileAccessToken->Write();
	q_updateFileAccessToken->Reset();
}

/**
* @-SQLGenAccess
* @func vector<SToken> ClientDAO::getFileAccessTokens
* @return int64 id, string accountname, string token, int is_user
* @sql
*    SELECT id, accountname, token, is_user
*    FROM fileaccess_tokens
*/
std::vector<ClientDAO::SToken> ClientDAO::getFileAccessTokens(void)
{
	if(q_getFileAccessTokens==nullptr)
	{
		q_getFileAccessTokens=db->Prepare("SELECT id, accountname, token, is_user FROM fileaccess_tokens", false);
	}
	db_results res=q_getFileAccessTokens->Read();
	std::vector<ClientDAO::SToken> ret;
	ret.resize(res.size());
	for(size_t i=0;i<res.size();++i)
	{
		ret[i].id=watoi64(res[i]["id"]);
		ret[i].accountname=res[i]["accountname"];
		ret[i].token=res[i]["token"];
		ret[i].is_user=watoi(res[i]["is_user"]);
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func int64 ClientDAO::getFileAccessTokenId2Alts
* @return int64 id
* @sql
*    SELECT id
*    FROM fileaccess_tokens WHERE accountname = :accountname(string) AND
*								  (is_user = :is_user_alt1(int) OR is_user = :is_user_alt2(int))
*/
ClientDAO::CondInt64 ClientDAO::getFileAccessTokenId2Alts(const std::string& accountname, int is_user_alt1, int is_user_alt2)
{
	if(q_getFileAccessTokenId2Alts==nullptr)
	{
		q_getFileAccessTokenId2Alts=db->Prepare("SELECT id FROM fileaccess_tokens WHERE accountname = ? AND (is_user = ? OR is_user = ?)", false);
	}
	q_getFileAccessTokenId2Alts->Bind(accountname);
	q_getFileAccessTokenId2Alts->Bind(is_user_alt1);
	q_getFileAccessTokenId2Alts->Bind(is_user_alt2);
	db_results res=q_getFileAccessTokenId2Alts->Read();
	q_getFileAccessTokenId2Alts->Reset();
	CondInt64 ret = { false, 0 };
	if(!res.empty())
	{
		ret.exists=true;
		ret.value=watoi64(res[0]["id"]);
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func int64 ClientDAO::getFileAccessTokenId
* @return int64 id
* @sql
*    SELECT id
*    FROM fileaccess_tokens WHERE accountname = :accountname(string) AND
*								  is_user = :is_user(int)
*/
ClientDAO::CondInt64 ClientDAO::getFileAccessTokenId(const std::string& accountname, int is_user)
{
	if(q_getFileAccessTokenId==nullptr)
	{
		q_getFileAccessTokenId=db->Prepare("SELECT id FROM fileaccess_tokens WHERE accountname = ? AND is_user = ?", false);
	}
	q_getFileAccessTokenId->Bind(accountname);
	q_getFileAccessTokenId->Bind(is_user);
	db_results res=q_getFileAccessTokenId->Read();
	q_getFileAccessTokenId->Reset();
	CondInt64 ret = { false, 0 };
	if(!res.empty())
	{
		ret.exists=true;
		ret.value=watoi64(res[0]["id"]);
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func void ClientDAO::removeGroupMembership
* @sql
*	 DELETE FROM token_group_memberships WHERE uid=:uid(int64)
*/
void ClientDAO::removeGroupMembership(int64 uid)
{
	if(q_removeGroupMembership==nullptr)
	{
		q_removeGroupMembership=db->Prepare("DELETE FROM token_group_memberships WHERE uid=?", false);
	}
	q_removeGroupMembership->Bind(uid);
	q_removeGroupMembership->Write();
	q_removeGroupMembership->Reset();
}

/**
* @-SQLGenAccess
* @func void ClientDAO::updateGroupMembership
* @sql
*    INSERT OR REPLACE INTO token_group_memberships
*          (uid, gid)
*      VALUES
*           (:uid(int64), (SELECT id FROM fileaccess_tokens WHERE accountname = :accountname(string) AND is_user=0) )
*/
void ClientDAO::updateGroupMembership(int64 uid, const std::string& accountname)
{
	if(q_updateGroupMembership==nullptr)
	{
		q_updateGroupMembership=db->Prepare("INSERT OR REPLACE INTO token_group_memberships (uid, gid) VALUES (?, (SELECT id FROM fileaccess_tokens WHERE accountname = ? AND is_user=0) )", false);
	}
	q_updateGroupMembership->Bind(uid);
	q_updateGroupMembership->Bind(accountname);
	q_updateGroupMembership->Write();
	q_updateGroupMembership->Reset();
}

/**
* @-SQLGenAccess
* @func vector<int> ClientDAO::getGroupMembership
* @return int gid
* @sql
*    SELECT gid
*    FROM token_group_memberships
*			WHERE uid = :uid(int)
*/
std::vector<int> ClientDAO::getGroupMembership(int uid)
{
	if(q_getGroupMembership==nullptr)
	{
		q_getGroupMembership=db->Prepare("SELECT gid FROM token_group_memberships WHERE uid = ?", false);
	}
	q_getGroupMembership->Bind(uid);
	db_results res=q_getGroupMembership->Read();
	q_getGroupMembership->Reset();
	std::vector<int> ret;
	ret.resize(res.size());
	for(size_t i=0;i<res.size();++i)
	{
		ret[i]=watoi(res[i]["gid"]);
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func void ClientDAO::addBackupDir
* @sql
*    INSERT INTO backupdirs
*		(name, path, server_default, optional, tgroup, symlinked, facet)
*    VALUES
*       (:name(string), :path(string), :server_default(int), :flags(int), :tgroup(int),
*        :symlinked(int), :facet(int) )
**/
void ClientDAO::addBackupDir(const std::string& name, const std::string& path, int server_default, int flags, int tgroup, int symlinked, int facet)
{
	if(q_addBackupDir==nullptr)
	{
		q_addBackupDir=db->Prepare("INSERT INTO backupdirs (name, path, server_default, optional, tgroup, symlinked, facet) VALUES (?, ?, ?, ?, ?, ?, ? )", false);
	}
	q_addBackupDir->Bind(name);
	q_addBackupDir->Bind(path);
	q_addBackupDir->Bind(server_default);
	q_addBackupDir->Bind(flags);
	q_addBackupDir->Bind(tgroup);
	q_addBackupDir->Bind(symlinked);
	q_addBackupDir->Bind(facet);
	q_addBackupDir->Write();
	q_addBackupDir->Reset();
}

/**
* @-SQLGenAccess
* @func void ClientDAO::delBackupDir
* @sql
*    DELETE FROM backupdirs WHERE id=:id(int64)
**/
void ClientDAO::delBackupDir(int64 id)
{
	if(q_delBackupDir==nullptr)
	{
		q_delBackupDir=db->Prepare("DELETE FROM backupdirs WHERE id=?", false);
	}
	q_delBackupDir->Bind(id);
	q_delBackupDir->Write();
	q_delBackupDir->Reset();
}

/**
* @-SQLGenAccess
* @func void ClientDAO::setResetKeep
* @sql
*    UPDATE backupdirs SET reset_keep=:val(int) WHERE id=:id(int64)
**/
void ClientDAO::setResetKeep(int val, int64 id)
{
	if(q_setResetKeep==nullptr)
	{
		q_setResetKeep=db->Prepare("UPDATE backupdirs SET reset_keep=? WHERE id=?", false);
	}
	q_setResetKeep->Bind(val);
	q_setResetKeep->Bind(id);
	q_setResetKeep->Write();
	q_setResetKeep->Reset();
}

/**
* @-SQLGenAccess
* @func void ClientDAO::resetHardlink
* @sql
*    DELETE FROM hardlinks WHERE vol=:vol(string) AND frn_high=:frn_high(int64) AND frn_low=:frn_low(int64)
**/
void ClientDAO::resetHardlink(const std::string& vol, int64 frn_high, int64 frn_low)
{
	if(q_resetHardlink==nullptr)
	{
		q_resetHardlink=db->Prepare("DELETE FROM hardlinks WHERE vol=? AND frn_high=? AND frn_low=?", false);
	}
	q_resetHardlink->Bind(vol);
	q_resetHardlink->Bind(frn_high);
	q_resetHardlink->Bind(frn_low);
	q_resetHardlink->Write();
	q_resetHardlink->Reset();
}

/**
* @-SQLGenAccess
* @func int64 ClientDAO::hasHardLink
* @return int64 frn_low
* @sql
*    SELECT frn_low FROM hardlinks WHERE vol=:vol(string) AND frn_high=:frn_high(int64) AND frn_low=:frn_low(int64) LIMIT 1
**/
ClientDAO::CondInt64 ClientDAO::hasHardLink(const std::string& vol, int64 frn_high, int64 frn_low)
{
	if(q_hasHardLink==nullptr)
	{
		q_hasHardLink=db->Prepare("SELECT frn_low FROM hardlinks WHERE vol=? AND frn_high=? AND frn_low=? LIMIT 1", false);
	}
	q_hasHardLink->Bind(vol);
	q_hasHardLink->Bind(frn_high);
	q_hasHardLink->Bind(frn_low);
	db_results res=q_hasHardLink->Read();
	q_hasHardLink->Reset();
	CondInt64 ret = { false, 0 };
	if(!res.empty())
	{
		ret.exists=true;
		ret.value=watoi64(res[0]["frn_low"]);
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func void ClientDAO::addHardlink
* @sql
*    INSERT OR IGNORE INTO hardlinks (vol, frn_high, frn_low, parent_frn_high, parent_frn_low)
*	 VALUES (:vol(string), :frn_high(int64), :frn_low(int64), :parent_frn_high(int64), :parent_frn_low(int64))
**/
void ClientDAO::addHardlink(const std::string& vol, int64 frn_high, int64 frn_low, int64 parent_frn_high, int64 parent_frn_low)
{
	if(q_addHardlink==nullptr)
	{
		q_addHardlink=db->Prepare("INSERT OR IGNORE INTO hardlinks (vol, frn_high, frn_low, parent_frn_high, parent_frn_low) VALUES (?, ?, ?, ?, ?)", false);
	}
	q_addHardlink->Bind(vol);
	q_addHardlink->Bind(frn_high);
	q_addHardlink->Bind(frn_low);
	q_addHardlink->Bind(parent_frn_high);
	q_addHardlink->Bind(parent_frn_low);
	q_addHardlink->Write();
	q_addHardlink->Reset();
}

/**
* @-SQLGenAccess
* @func void ClientDAO::resetAllHardlinks
* @sql
*    DELETE FROM hardlinks
**/
void ClientDAO::resetAllHardlinks(void)
{
	if(q_resetAllHardlinks==nullptr)
	{
		q_resetAllHardlinks=db->Prepare("DELETE FROM hardlinks", false);
	}
	q_resetAllHardlinks->Write();
}

/**
* @-SQLGenAccess
* @func SClientFacet ClientDAO::getClientFacet
* @return int id, string name, string server_identity
* @sql
*    SELECT id, name, server_identity FROM client_facets WHERE server_identity=:server_identity(string)
**/
ClientDAO::SClientFacet ClientDAO::getClientFacet(const std::string& server_identity)
{
	if(q_getClientFacet==nullptr)
	{
		q_getClientFacet=db->Prepare("SELECT id, name, server_identity FROM client_facets WHERE server_identity=?", false);
	}
	q_getClientFacet->Bind(server_identity);
	db_results res=q_getClientFacet->Read();
	q_getClientFacet->Reset();
	SClientFacet ret = { false, 0, "", "" };
	if(!res.empty())
	{
		ret.exists=true;
		ret.id=watoi(res[0]["id"]);
		ret.name=res[0]["name"];
		ret.server_identity=res[0]["server_identity"];
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func SClientFacet ClientDAO::getClientFacetByName
* @return int id, string name, string server_identity
* @sql
*    SELECT id, name, server_identity FROM client_facets WHERE name=:name(string)
**/
ClientDAO::SClientFacet ClientDAO::getClientFacetByName(const std::string& name)
{
	if(q_getClientFacetByName==nullptr)
	{
		q_getClientFacetByName=db->Prepare("SELECT id, name, server_identity FROM client_facets WHERE name=?", false);
	}
	q_getClientFacetByName->Bind(name);
	db_results res=q_getClientFacetByName->Read();
	q_getClientFacetByName->Reset();
	SClientFacet ret = { false, 0, "", "" };
	if(!res.empty())
	{
		ret.exists=true;
		ret.id=watoi(res[0]["id"]);
		ret.name=res[0]["name"];
		ret.server_identity=res[0]["server_identity"];
	}
	return ret;
}

/**
* @-SQLGenAccess
* @func void ClientDAO::addClientFacet
* @sql
*    INSERT INTO client_facets (name, server_identity) VALUES (:name(string), :server_identity(string))
**/
void ClientDAO::addClientFacet(const std::string& name, const std::string& server_identity)
{
	if(q_addClientFacet==nullptr)
	{
		q_addClientFacet=db->Prepare("INSERT INTO client_facets (name, server_identity) VALUES (?, ?)", false);
	}
	q_addClientFacet->Bind(name);
	q_addClientFacet->Bind(server_identity);
	q_addClientFacet->Write();
	q_addClientFacet->Reset();
}

/**
* @-SQLGenAccess
* @func void ClientDAO::updateClientFacet
* @sql
*    UPDATE client_facets SET server_identity=:server_identity(string) WHERE id=:id(int)
**/
void ClientDAO::updateClientFacet(const std::string& server_identity, int id)
{
	if(q_updateClientFacet==nullptr)
	{
		q_updateClientFacet=db->Prepare("UPDATE client_facets SET server_identity=? WHERE id=?", false);
	}
	q_updateClientFacet->Bind(server_identity);
	q_updateClientFacet->Bind(id);
	q_updateClientFacet->Write();
	q_updateClientFacet->Reset();
}

//-------------------

std::vector<std::pair<int, std::string> > getFlagStrMapping()
{
	std::vector<std::pair<int, std::string> > flag_mapping;
	flag_mapping.push_back(std::make_pair(EBackupDirFlag_Optional, "optional"));
	flag_mapping.push_back(std::make_pair(EBackupDirFlag_FollowSymlinks, "follow_symlinks"));
	flag_mapping.push_back(std::make_pair(EBackupDirFlag_SymlinksOptional, "symlinks_optional"));
	flag_mapping.push_back(std::make_pair(EBackupDirFlag_OneFilesystem, "one_filesystem"));
	flag_mapping.push_back(std::make_pair(EBackupDirFlag_RequireSnapshot, "require_snapshot"));
	flag_mapping.push_back(std::make_pair(EBackupDirFlag_KeepFiles, "keep"));
	flag_mapping.push_back(std::make_pair(EBackupDirFlag_ShareHashes, "share_hashes"));
	flag_mapping.push_back(std::make_pair(EBackupDirFlag_Required, "required"));
	flag_mapping.push_back(std::make_pair(EBackupDirFlag_IncludeDirectorySymlinks, "include_dir_symlinks"));
	return flag_mapping;
}
