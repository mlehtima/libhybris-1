#ifndef EGLNATIVEWINDOWBASE_H
#define EGLNATIVEWINDOWBASE_H

/* for ICS window.h */
#include <string.h>
#include <system/window.h>
#include "nativewindowbase.h"
#include <EGL/egl.h>
#include <stdarg.h>
#include <assert.h>

#ifdef DEBUG
#include <stdio.h>
#endif

#define NO_ERROR                0L
#define BAD_VALUE               -1

/**
 * @brief A Class to do common ANativeWindow initialization and thunk c-style
 *        callbacks into C++ method calls.
 **/
class EGLBaseNativeWindow : public BaseNativeWindow
{
public:
	operator EGLNativeWindowType()
	{
		EGLNativeWindowType ret = reinterpret_cast<EGLNativeWindowType>(static_cast<ANativeWindow *>(this));
		return ret;
	}
};

#endif
