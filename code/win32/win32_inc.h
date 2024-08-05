#ifndef WIN32_INC_H
#define WIN32_INC_H

#define _WIN32_WINNT 0x0601 // @Note: This means, we're targeting Windows 7 or later.
#include <sdkddkver.h>

#ifndef FULL_WIN32
# define NOGDICAPMASKS
# define NOMENUS
# define NOICONS
# define NOSYSCOMMANDS
# define OEMRESOURCE
# define NOATOM
# define NOCLIPBOARD
# define NOCOLOR
# define NODRAWTEXT
# define NOKERNEL
# define NONLS
# define NOMEMMGR
# define NOMETAFILE
# define NOOPENFILE
# define NOSCROLL
# define NOSERVICE
# define NOSOUND
# define NOTEXTMETRIC
# define NOWH
# define NOCOMM
# define NOKANJI
# define NOHELP
# define NOPROFILER
# define NODEFERWINDOWPOS
# define NOMCX
# define NORPC
# define NOPROXYSTUB
# define NOIMAGE
# define NOTAPE
# define STRICT
#endif // FULL_WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commdlg.h>

#endif // WIN32_INC_H
