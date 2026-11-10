#ifndef TSUGARU_UWP_IFSTREAM_IS_INCLUDED
#define TSUGARU_UWP_IFSTREAM_IS_INCLUDED
/* File streams for the UWP build.
   Two things the CRT's std::ifstream/std::ofstream get wrong there: (1) the narrow path is read in the ANSI code page,
   so a UTF-8 path with accents ("Cainã") opens the wrong name; (2) on the console a folder the user picked (a USB
   drive) can only be opened through the broker functions (CreateFile2FromAppW...).  The UWP host's compat layer offers
   swanui_fopen_u8 (UTF-8, ordinary open first, broker fallback, every mode); these classes wrap its FILE* in standard
   streams.  In every other build the Tsugaru* names are the std:: streams. */

#include <fstream>
#include <istream>
#include <ostream>
#include <streambuf>
#include <string>
#include <stdio.h>
#include <string.h>
#include <algorithm>

#ifdef TSUGARU_UWP

extern "C" FILE *swanui_fopen_u8(const char *path,const char *mode);

namespace tsugaru_uwp
{

/* A streambuf over a FILE*.  Reading is buffered here (get area); writing goes straight to fwrite (the CRT buffers). */
class FileBuf : public std::streambuf
{
private:
	FILE *fp=nullptr;
	char buf[16384];
	bool writable=false;

public:
	~FileBuf()
	{
		Close();
	}
	bool Open(const std::string &fName,std::ios_base::openmode m)
	{
		Close();
		const bool bin=0!=(m&std::ios_base::binary);
		const bool in=0!=(m&std::ios_base::in);
		const bool out=0!=(m&std::ios_base::out);
		const bool app=0!=(m&std::ios_base::app);
		const bool trunc=0!=(m&std::ios_base::trunc);
		std::string mode;
		if(in && out)
		{
			mode=(trunc ? "w+" : "r+");
		}
		else if(out)
		{
			mode=(app ? "a" : "w");
		}
		else
		{
			mode="r";
		}
		if(bin)
		{
			mode+="b";
		}
		fp=swanui_fopen_u8(fName.c_str(),mode.c_str());
		writable=out;
		setg(buf,buf,buf);
		return nullptr!=fp;
	}
	bool IsOpen(void) const
	{
		return nullptr!=fp;
	}
	void Close(void)
	{
		if(nullptr!=fp)
		{
			fclose(fp);
			fp=nullptr;
		}
		setg(buf,buf,buf);
	}

protected:
	// The read-ahead in the get area makes the OS position run ahead of the stream position; before any write or
	// seek it is dropped and the OS position corrected.
	void DropReadAhead(void)
	{
		const long long ahead=(long long)(egptr()-gptr());
		if(0<ahead && nullptr!=fp)
		{
			_fseeki64(fp,-ahead,SEEK_CUR);
		}
		setg(buf,buf,buf);
	}
	int_type underflow(void) override
	{
		if(nullptr==fp)
		{
			return traits_type::eof();
		}
		if(gptr()<egptr())
		{
			return traits_type::to_int_type(*gptr());
		}
		size_t n=fread(buf,1,sizeof(buf),fp);
		if(0==n)
		{
			return traits_type::eof();
		}
		setg(buf,buf,buf+n);
		return traits_type::to_int_type(*gptr());
	}
	std::streamsize xsgetn(char *s,std::streamsize count) override
	{
		std::streamsize done=0;
		while(done<count)
		{
			std::streamsize avail=egptr()-gptr();
			if(0<avail)
			{
				std::streamsize n=std::min(avail,count-done);
				memcpy(s+done,gptr(),(size_t)n);
				gbump((int)n);
				done+=n;
			}
			else if(sizeof(buf)<=(size_t)(count-done) && nullptr!=fp)
			{
				size_t n=fread(s+done,1,(size_t)(count-done),fp);
				done+=n;
				if(0==n)
				{
					break;
				}
			}
			else if(traits_type::eof()==underflow())
			{
				break;
			}
		}
		return done;
	}
	int_type overflow(int_type c) override
	{
		if(nullptr==fp || !writable)
		{
			return traits_type::eof();
		}
		DropReadAhead();
		if(traits_type::eof()!=c)
		{
			const char ch=traits_type::to_char_type(c);
			if(1!=fwrite(&ch,1,1,fp))
			{
				return traits_type::eof();
			}
		}
		return traits_type::not_eof(c);
	}
	std::streamsize xsputn(const char *s,std::streamsize count) override
	{
		if(nullptr==fp || !writable)
		{
			return 0;
		}
		DropReadAhead();
		return (std::streamsize)fwrite(s,1,(size_t)count,fp);
	}
	int sync(void) override
	{
		return (nullptr!=fp && 0==fflush(fp)) ? 0 : -1;
	}
	pos_type seekoff(off_type off,std::ios_base::seekdir dir,std::ios_base::openmode) override
	{
		if(nullptr==fp)
		{
			return pos_type(off_type(-1));
		}
		int origin=SEEK_SET;
		if(std::ios_base::cur==dir)
		{
			origin=SEEK_CUR;
		}
		else if(std::ios_base::end==dir)
		{
			origin=SEEK_END;
		}
		DropReadAhead();
		if(0!=_fseeki64(fp,(__int64)off,origin))
		{
			return pos_type(off_type(-1));
		}
		return pos_type((off_type)_ftelli64(fp));
	}
	pos_type seekpos(pos_type pos,std::ios_base::openmode which) override
	{
		return seekoff((off_type)pos,std::ios_base::beg,which);
	}
};

template <class STREAM,std::ios_base::openmode DEFAULT_MODE>
class FileStream : public STREAM
{
private:
	FileBuf fileBuf;

public:
	FileStream() : STREAM(nullptr)
	{
		this->init(&fileBuf);
	}
	explicit FileStream(const std::string &fName,std::ios_base::openmode m=DEFAULT_MODE) : STREAM(nullptr)
	{
		this->init(&fileBuf);
		open(fName,m);
	}
	explicit FileStream(const char *fName,std::ios_base::openmode m=DEFAULT_MODE) : STREAM(nullptr)
	{
		this->init(&fileBuf);
		open(std::string(fName),m);
	}
	FileStream(const FileStream &)=delete;
	FileStream &operator=(const FileStream &)=delete;

	void open(const std::string &fName,std::ios_base::openmode m=DEFAULT_MODE)
	{
		this->clear();
		if(true!=fileBuf.Open(fName,m|DEFAULT_MODE))
		{
			this->setstate(std::ios_base::failbit);
		}
	}
	void open(const char *fName,std::ios_base::openmode m=DEFAULT_MODE)
	{
		open(std::string(fName),m);
	}
	bool is_open(void) const
	{
		return fileBuf.IsOpen();
	}
	void close(void)
	{
		fileBuf.Close();
	}
};

typedef FileStream <std::istream,std::ios_base::in> IfStream;
typedef FileStream <std::ostream,std::ios_base::out> OfStream;
typedef FileStream <std::iostream,std::ios_base::in|std::ios_base::out> FStream;

}

typedef tsugaru_uwp::IfStream TsugaruIfStream;
typedef tsugaru_uwp::OfStream TsugaruOfStream;
typedef tsugaru_uwp::FStream TsugaruFStream;

#else

typedef std::ifstream TsugaruIfStream;
typedef std::ofstream TsugaruOfStream;
typedef std::fstream TsugaruFStream;

#endif

#endif
