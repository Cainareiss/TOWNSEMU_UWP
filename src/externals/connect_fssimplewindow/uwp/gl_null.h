#ifndef TSUGARU_UWP_GL_NULL_IS_INCLUDED
#define TSUGARU_UWP_GL_NULL_IS_INCLUDED
/* The UWP build has no OpenGL.  The window connection (fssimplewindow_connection.cpp) still calls the handful of
   fixed-function GL entry points to draw the picture; in the UWP build the host application draws the picture
   itself (the connection hands it over with TsugaruUwpPublishFrame), so these calls do nothing.
   Force-included (/FI) into the connection only. */

typedef unsigned int GLuint;
typedef unsigned int GLenum;
typedef int GLint;
typedef int GLsizei;
typedef float GLfloat;
typedef unsigned int GLbitfield;
typedef double GLdouble;
typedef unsigned char GLubyte;

#define GL_TEXTURE_2D 0x0DE1
#define GL_CLAMP 0x2900
#define GL_NEAREST 0x2600
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_QUADS 0x0007
#define GL_LINES 0x0001
#define GL_TRIANGLE_FAN 0x0006
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_PROJECTION 0x1701
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_MODULATE 0x2100
#define GL_COLOR_BUFFER_BIT 0x4000

static inline void glVertex2i(GLint,GLint){}
static inline void glBindTexture(GLenum,GLuint){}
static inline void glColor3ub(GLubyte,GLubyte,GLubyte){}
static inline void glColor3f(GLfloat,GLfloat,GLfloat){}
static inline void glTexParameteri(GLenum,GLenum,GLint){}
static inline void glTexCoord2f(GLfloat,GLfloat){}
static inline void glEnd(void){}
static inline void glBegin(GLenum){}
static inline void glDeleteTextures(GLsizei,const GLuint *){}
static inline void glGenTextures(GLsizei n,GLuint *ids){for(GLsizei i=0; i<n; ++i){ids[i]=(GLuint)(i+1);}}
static inline void glTexImage2D(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void *){}
static inline void glTexEnvi(GLenum,GLenum,GLint){}
static inline void glOrtho(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble){}
static inline void glMatrixMode(GLenum){}
static inline void glLoadIdentity(void){}
static inline void glEnable(GLenum){}
static inline void glDisable(GLenum){}
static inline void glClearColor(GLfloat,GLfloat,GLfloat,GLfloat){}
static inline void glClear(GLbitfield){}
static inline void glViewport(GLint,GLint,GLsizei,GLsizei){}

#endif
