#include <iostream>
#include "filesys.h"
#include "cpputil.h"



FileSys::FindStruct::FindStruct()
{
	findContext=CreateFindContext();
}
FileSys::FindStruct::~FindStruct()
{
	if(nullptr!=findContext)
	{
		DeleteFindContext(findContext);
		findContext=nullptr;
	}
}
void FileSys::HasTimeStamp::ClearTimeStamp(void)
{
	year=0;
	month=0;
	day=0;
	hours=0;
	minutes=0;
	seconds=0;
}
void FileSys::HasTimeStamp::CopyTimeStampFrom(const HasTimeStamp &incoming)
{
	year=incoming.year;
	month=incoming.month;
	day=incoming.day;
	hours=incoming.hours;
	minutes=incoming.minutes;
	seconds=incoming.seconds;
}
int FileSys::FindFirst(DirectoryEntry &ent,unsigned int PSP,const std::string &subPath)
{
	auto fsIdx=FindAvailableFindStruct(subPath);
	if(0<=fsIdx)
	{
		ent=FindFirst(subPath,findStruct[fsIdx].findContext);
		if(true!=ent.endOfDir)
		{
			findStruct[fsIdx].PSP=PSP;
			findStruct[fsIdx].usedTime=++usedTimeSeed;
			findStruct[fsIdx].used=true;
			findStruct[fsIdx].subPath=subPath;
			return fsIdx;
		}
	}
	return -1;
}
FileSys::DirectoryEntry FileSys::FindNext(int fsIdx)
{
	DirectoryEntry ent;
	if(true==findStruct[fsIdx].used)
	{
		ent=FindNext(findStruct[fsIdx].findContext);
		if(true==ent.endOfDir)
		{
			findStruct[fsIdx].used=false;
		}
	}
	else
	{
		ent.endOfDir=true;
	}
	return ent;
}
bool FileSys::FindStructValid(int findStructIdx) const
{
	if(0<=findStructIdx && findStructIdx<MAX_NUM_OPEN_DIRECTORY)
	{
		return findStruct[findStructIdx].used;
	}
	return false;
}
int FileSys::FindAvailableFindStruct(const std::string &subPath) const
{
	for(int i=0; i<MAX_NUM_OPEN_DIRECTORY; ++i)
	{
		if(true!=findStruct[i].used)
		{
			return i;
		}
	}

	// If nothing is found:
	int sameName=-1;
	for(int i=0; i<MAX_NUM_OPEN_DIRECTORY; ++i)
	{
		if(findStruct[i].subPath==subPath)
		{
			if(sameName<0)
			{
				sameName=i;
			}
			else if(findStruct[i].usedTime<findStruct[sameName].usedTime)
			{
				sameName=i;
			}
		}
	}
	if(0<=sameName)
	{
		return sameName;
	}

	// If nothing is still found:
	int oldest=0,newest=0;
	for(int i=1; i<MAX_NUM_OPEN_DIRECTORY; ++i)
	{
		if(findStruct[i].usedTime<findStruct[oldest].usedTime)
		{
			oldest=i;
		}
		else
		{
			newest=i;
		}
	}
	// Denomination.
	auto oldestTime=findStruct[oldest].usedTime;
	for(int i=0; i<MAX_NUM_OPEN_DIRECTORY; ++i)
	{
		findStruct[i].usedTime-=oldestTime;
	}
	usedTimeSeed=findStruct[newest].usedTime+1;

	return oldest;
}


////////////////////////////////////////////////////////////


uint16_t FileSys::HasTimeStamp::FormatDOSTime(void) const
{
	return (hours<<11)|(minutes<<5)|(seconds/2);
}
uint16_t FileSys::HasTimeStamp::FormatDOSDate(void) const
{
	return ((year-1980)<<9)|(month<<5)|(day);
}
bool FileSys::SystemFileTable::IsOpen(void) const
{
	return fp.is_open();
}
const uint32_t FileSys::SystemFileTable::GetFileSize(void)
{
	if(true==fp.eof())
	{
		fp.clear(); // Otherwise, cannot seek, tellg() will return -1.
	}
	auto curPos=fp.tellg();

	fp.seekg(0,std::ios::end);
	auto sz=fp.tellg();

	fp.seekg(curPos,std::ios::beg);

	return sz;
}
const uint32_t FileSys::SystemFileTable::GetFilePointer(void)
{
	if(true==fp.eof())
	{
		fp.clear(); // Otherwise, cannot seek, tellg() will return -1.
	}
	return fp.tellg();
}

std::vector <unsigned char> FileSys::SystemFileTable::Read(uint32_t len)
{
	std::vector <unsigned char> buf;
	if(true==fp.is_open())
	{
		buf.resize(len);
		fp.read((char *)buf.data(),len);

		auto actualRead=fp.gcount();
		if(actualRead<len)
		{
			buf.resize(actualRead);
		}
	}
	return buf;
}
uint32_t FileSys::SystemFileTable::Write(const std::vector <unsigned char> &data)
{
	if(true==fp.is_open())
	{
		auto ptr0=fp.tellp();
		fp.write((const char *)data.data(),data.size());
		auto ptr1=fp.tellp();
		return ptr1-ptr0; // Seriously? gcount only works for read?
	}
	return 0;
}
bool FileSys::SubPathIsDirectory(std::string subPath)
{
	BackSlashToSlash(subPath);
	auto findContext=CreateFindContext();
	auto dirent=FindFirst(subPath,findContext);
	DeleteFindContext(findContext);
	return 0!=(dirent.attr&ATTR_DIR);
}
int FileSys::OpenExistingFile(unsigned int PSP,std::string subPath,unsigned int openMode)
{
	auto sftIdx=FindAvailableSFT();
	if(0<=sftIdx)
	{
		BackSlashToSlash(subPath);

		auto fullPath=cpputil::MakeFullPathName(hostPath,subPath);
		if(true!=cpputil::FileExists(fullPath))
		{
			return -1;
		}

		auto dirent=GetFileAttrib(subPath);

		sft[sftIdx].fName=subPath;
		sft[sftIdx].mode=openMode;
		sft[sftIdx].PSP=PSP;
		switch(openMode)
		{
		case OPENMODE_READ:
			sft[sftIdx].fp.open(fullPath,std::ios::in|std::ios::binary);
			break;
		case OPENMODE_WRITE:
			sft[sftIdx].fp.open(fullPath,std::ios::out|std::ios::binary);
			break;
		case OPENMODE_RW:
			sft[sftIdx].fp.open(fullPath,std::ios::in|std::ios::out|std::ios::binary);
			break;
		}

		if(true==sft[sftIdx].fp.is_open())
		{
			sft[sftIdx].CopyTimeStampFrom(dirent);
			return sftIdx;
		}
	}
	return -1;
}
int FileSys::OpenFileNotTruncate(unsigned int PSP,std::string subPath,unsigned int openMode)
{
	auto sftIdx=FindAvailableSFT();
	if(0<=sftIdx)
	{
		BackSlashToSlash(subPath);

		auto fullPath=cpputil::MakeFullPathName(hostPath,subPath);
		if(true!=cpputil::FileExists(fullPath))
		{
			// If not exist, make one.
			std::ofstream fp(fullPath,std::ios::binary);
		}

		auto dirent=GetFileAttrib(subPath);

		sft[sftIdx].fName=subPath;
		sft[sftIdx].mode=openMode;
		sft[sftIdx].PSP=PSP;
		switch(openMode)
		{
		case OPENMODE_READ:
			sft[sftIdx].fp.open(fullPath,std::ios::in|std::ios::binary);
			break;
		case OPENMODE_WRITE: // Not to truncate, use in/out mode for WRITE.
		case OPENMODE_RW:
			sft[sftIdx].fp.open(fullPath,std::ios::in|std::ios::out|std::ios::binary);
			break;
		}
		if(true==sft[sftIdx].fp.is_open())
		{
			sft[sftIdx].CopyTimeStampFrom(dirent);
			return sftIdx;
		}
	}
	return -1;
}
int FileSys::OpenFileTruncate(unsigned int PSP,std::string subPath,unsigned int openMode)
{
	if(OPENMODE_READ==openMode)
	{
		return -1;
	}

	auto sftIdx=FindAvailableSFT();
	if(0<=sftIdx)
	{
		BackSlashToSlash(subPath);

		auto fullPath=cpputil::MakeFullPathName(hostPath,subPath);
		if(true!=cpputil::FileExists(fullPath))
		{
			// If not exist, make one.
			std::ofstream fp(fullPath,std::ios::binary);
		}

		auto dirent=GetFileAttrib(subPath);

		sft[sftIdx].fName=subPath;
		sft[sftIdx].mode=openMode;
		sft[sftIdx].PSP=PSP;
		sft[sftIdx].fp.open(fullPath,std::ios::in|std::ios::out|std::ios::trunc|std::ios::binary);

		if(true==sft[sftIdx].fp.is_open())
		{
			sft[sftIdx].CopyTimeStampFrom(dirent);
			return sftIdx;
		}
	}
	return -1;
}
bool FileSys::CheckFileExist(std::string subPath)
{
	BackSlashToSlash(subPath);
	auto fullPath=cpputil::MakeFullPathName(hostPath,subPath);
	return cpputil::FileExists(fullPath);
}
void FileSys::Seek(int sftIdx,uint32_t pos)
{
	if(0<=sftIdx &&
	   sftIdx<MAX_NUM_OPEN_FILE &&
	   true==sft[sftIdx].fp.is_open())
	{
		if(true==sft[sftIdx].fp.eof())
		{
			sft[sftIdx].fp.clear(); // Otherwise, cannot seek, tellg() will return -1.
		}
		switch(sft[sftIdx].mode)
		{
		case OPENMODE_READ://   // Keep this number.  Compatible with DOS SFT
			sft[sftIdx].fp.seekg(pos,std::ios::beg);
			break;
		case OPENMODE_WRITE://  // Keep this number.  Compatible with DOS SFT
			sft[sftIdx].fp.seekp(pos,std::ios::beg);
			break;
		case OPENMODE_RW://     // Keep this number.  Compatible with DOS SFT
			sft[sftIdx].fp.seekg(pos,std::ios::beg);
			sft[sftIdx].fp.seekp(pos,std::ios::beg);
			break;
		}
	}
}

uint32_t FileSys::Fsize(int sftIdx)
{
	uint32_t sz=0;
	if(0<=sftIdx &&
	   sftIdx<MAX_NUM_OPEN_FILE &&
	   true==sft[sftIdx].fp.is_open())
	{
		if(true==sft[sftIdx].fp.eof())
		{
			sft[sftIdx].fp.clear(); // Otherwise, cannot seek, tellg() will return -1.
		}

		switch(sft[sftIdx].mode)
		{
		case OPENMODE_READ://   // Keep this number.  Compatible with DOS SFT
			{
				auto curPos=sft[sftIdx].fp.tellg();

				sft[sftIdx].fp.seekg(0,std::ios::end);
				sz=(uint32_t)sft[sftIdx].fp.tellg();

				sft[sftIdx].fp.seekg(curPos,std::ios::beg);
			}
			break;
		case OPENMODE_WRITE://  // Keep this number.  Compatible with DOS SFT
		case OPENMODE_RW://     // Keep this number.  Compatible with DOS SFT
			{
				auto curPos=sft[sftIdx].fp.tellp();

				sft[sftIdx].fp.seekp(0,std::ios::end);
				sz=(uint32_t)sft[sftIdx].fp.tellp();

				sft[sftIdx].fp.seekp(curPos,std::ios::beg);
			}
			break;
		}
	}
	return sz;
}

void FileSys::TruncateToSize(int sftIdx,uint32_t pos)
{
	uint32_t sz=0;
	if(0<=sftIdx &&
	   sftIdx<MAX_NUM_OPEN_FILE &&
	   true==sft[sftIdx].fp.is_open() &&
	   (OPENMODE_WRITE==sft[sftIdx].mode || OPENMODE_RW==sft[sftIdx].mode))
	{
		std::vector <unsigned char> data;

		sft[sftIdx].fp.close();

		auto fullPath=cpputil::MakeFullPathName(hostPath,sft[sftIdx].fName);
		if(0<pos)
		{
			sft[sftIdx].fp.open(fullPath,std::ios::in|std::ios::binary);
			if(sft[sftIdx].fp.is_open())
			{
				data.resize(pos);
				for(auto &d : data)
				{
					d=0;
				}
				sft[sftIdx].fp.read((char *)data.data(),pos);
				sft[sftIdx].fp.close();
			}
		}

		sft[sftIdx].fp.open(fullPath,std::ios::in|std::ios::out|std::ios::trunc|std::ios::binary);
		sft[sftIdx].fp.write((char *)data.data(),data.size());
		// Leave it open.
	}
}

bool FileSys::CloseFile(int sftIdx)
{
	if(0<=sftIdx &&
	   sftIdx<MAX_NUM_OPEN_FILE &&
	   true==sft[sftIdx].fp.is_open())
	{
		sft[sftIdx].fp.close();
		return true;
	}
	return false;
}
bool FileSys::RenameSubPath(std::string subPathFrom,std::string subPathTo)
{
	BackSlashToSlash(subPathFrom);
	BackSlashToSlash(subPathTo);

	auto fullPathFrom=cpputil::MakeFullPathName(hostPath,subPathFrom);
	auto fullPathTo=cpputil::MakeFullPathName(hostPath,subPathTo);
	return 0==rename(fullPathFrom.c_str(),fullPathTo.c_str());
}
bool FileSys::DeleteSubPathFile(std::string subPath)
{
	BackSlashToSlash(subPath);
	auto fullPath=cpputil::MakeFullPathName(hostPath,subPath);
	return 0==remove(fullPath.c_str());
}
bool FileSys::RmdirSubPath(std::string subPath)
{
	BackSlashToSlash(subPath);
	return Rmdir(cpputil::MakeFullPathName(hostPath,subPath));
}
bool FileSys::MkdirSubPath(std::string subPath)
{
	BackSlashToSlash(subPath);
	return Mkdir(cpputil::MakeFullPathName(hostPath,subPath));
}
int FileSys::FindAvailableSFT(void) const
{
	for(int i=0; i<MAX_NUM_OPEN_FILE; ++i)
	{
		if(true!=sft[i].fp.is_open())
		{
			return i;
		}
	}
	return -1;
}
void FileSys::CloseAllForPSP(unsigned int PSP)
{
	if(true==linked)
	{
		for(auto &fs : findStruct)
		{
			if(PSP==fs.PSP && true==fs.used)
			{
				FindClose(fs.findContext);
				fs.used=false;
			}
		}
		for(auto &t : sft)
		{
			if(PSP==t.PSP && true==t.fp.is_open())
			{
				t.fp.close();
			}
		}
	}
}

/*! Template="FILENAMEEXT" 11-letter format
    File=Normal
*/
/* static */ bool FileSys::DOSTemplateMatch(const std::string &templ11,const std::string &fName11)
{
	for(int i=0; i<11; ++i)
	{
		if('?'==templ11[i])
		{
			// Pass
		}
		else if(cpputil::Capitalize(templ11[i])!=cpputil::Capitalize(fName11[i]))
		{
			return false;
		}
	}
	return true;
}

/* static */ bool FileSys::DOSAttrMatch(unsigned int sAttr,unsigned int fAttr)
{
	// Absurd logic of MS-DOS attribute matching.
	if(0!=(sAttr&ATTR_VOLUME))
	{
		if(0==(fAttr&ATTR_VOLUME))
		{
			return false;
		}
		return true;
	}
	else
	{
		if(0!=(fAttr&ATTR_VOLUME))
		{
			return false;
		}
		sAttr&=~(0x40|ATTR_ARCHIVE|ATTR_READONLY);
		if(0==((~sAttr)&fAttr&(ATTR_DIR|ATTR_SYSTEM|ATTR_HIDDEN)))
		{
			return true;
		}
	}
	return false;
}

std::string FileSys::MakeHostPath(const std::string &subPath) const
{
	std::string path=hostPath;
	if(""!=subPath && "/"!=subPath && "\\"!=subPath)
	{
		if(""==path || (path.back()!='/' && path.back()!='\\'))
		{
			path.push_back('/');
		}
		path+=subPath;
	}
	for(auto &c : path)
	{
		if('\\'==c)
		{
			c='/';
		}
	}
	return path;
}

void FileSys::BackSlashToSlash(std::string &src)
{
	for(auto &c : src)
	{
		if('\\'==c)
		{
			c='/';
		}
	}
}