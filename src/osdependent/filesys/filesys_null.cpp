#include "filesys.h"



FileSys::FileSys()
{
}
FileSys::~FileSys()
{
}
/* static */ std::string FileSys::ToHostEncoding(std::string sjis)
{
	return sjis;
}
/* static */ std::string FileSys::ToSJISEncoding(std::string host)
{
	return host;
}
/* static */ std::string FileSys::Getcwd(void)
{
	return "";
}
/* static */ bool FileSys::Chdir(std::string str)
{
	return false;
}
/* static */ bool FileSys::Mkdir(std::string str)
{
	return false;
}

// The shared-directory feature (FileSys) is not available in the UWP build: nothing to list.
class FileSys::FindContext
{
};
/* static */ FileSys::FindContext *FileSys::CreateFindContext(void)
{
	return new FindContext;
}
/* static */ void FileSys::DeleteFindContext(FindContext *find)
{
	delete find;
}
/* static */ void FileSys::FindClose(FindContext *)
{
}
FileSys::DirectoryEntry FileSys::FindFirst(std::string,FindContext *)
{
	DirectoryEntry ent;
	ent.endOfDir=true;
	return ent;
}
FileSys::DirectoryEntry FileSys::FindNext(FindContext *)
{
	DirectoryEntry ent;
	ent.endOfDir=true;
	return ent;
}
FileSys::DirectoryEntry FileSys::GetFileAttrib(std::string) const
{
	DirectoryEntry ent;
	ent.endOfDir=true;
	return ent;
}
/* static */ bool FileSys::Rmdir(std::string)
{
	return false;
}
