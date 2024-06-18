/*************************************************************************
*    UrBackup - Client/Server backup system
*    Copyright (C) 2021 Martin Raiber
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

#include "BtrfsFactory.h"
#include "BackupFileSystem.h"

IBackupFileSystem* BtrfsFactory::openBtrfsImage(IFile* img)
{
	BtrfsBackupFileSystem* ret = new BtrfsBackupFileSystem(img);
	if (ret->hasError())
	{
		delete ret;
		return nullptr;
	}

	return ret;
}

bool BtrfsFactory::formatVolume(IFile* img)
{
	return btrfs_format_vol(img);
}
