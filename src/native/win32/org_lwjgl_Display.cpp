/* 
 * Copyright (c) 2002 Light Weight Java Game Library Project
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are 
 * met:
 * 
 * * Redistributions of source code must retain the above copyright 
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * * Neither the name of 'Light Weight Java Game Library' nor the names of 
 *   its contributors may be used to endorse or promote products derived 
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
/**
 * $Id$
 *
 * Win32 specific library for display handdling.
 *
 * @author cix_foo <cix_foo@users.sourceforge.net>
 * @version $Revision$
 */

#include <windows.h>
#include "org_lwjgl_Display.h"

#undef  DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0300

#include <dinput.h>

#define WINDOWCLASSNAME "LWJGLWINDOW"

extern HINSTANCE dll_handle;
// Initialise static variables
bool			oneShotInitialised = false;
HWND			hwnd = NULL;						// Handle to the window
HDC				hdc = NULL;							// Device context
LPDIRECTINPUT	lpdi = NULL;

void destroyDI(void)
{
	lpdi->Release();
	lpdi = NULL;
}

void destroyWindow(void)
{
	// Reset the display if necessary
	ChangeDisplaySettings(NULL, 0);

	if (hwnd != NULL) {
		// Vape the window
		DestroyWindow(hwnd);
		hwnd = NULL;
	}

#ifdef _DEBUG
	printf("Destroyed display\n");
#endif

	// Show the mouse
	ShowCursor(TRUE);
}

void destroyAll(void)
{
	destroyDI();
	destroyWindow();
}

void dumpLastError(void) {
	LPVOID lpMsgBuf;
	FormatMessage( 
		FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		FORMAT_MESSAGE_FROM_SYSTEM | 
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		0, // Default language
		(LPTSTR) &lpMsgBuf,
		0,
		NULL
	);
	printf("System error: %s\n", lpMsgBuf);
	LocalFree(lpMsgBuf);
}

/*
 *	A dummy WindowProc which does nothing. Used so we can have an invisible OpenGL window
 */
LRESULT CALLBACK WindowProc(HWND hWnd,
							UINT msg,
							WPARAM wParam,
							LPARAM lParam)
{
	switch (msg) {
		// disable screen saver and monitor power down messages which wreak havoc
		case WM_SYSCOMMAND:
		{
			switch (wParam) {
			case SC_SCREENSAVE:
			case SC_MONITORPOWER:
				return 0;
				break;
			default:
				break;
			}
		}
		case WM_PAINT:
			return 0;
	}

	// default action
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

/*
 * Sets the fullscreen display mode.
 * Returns 1 for success and -1 for failure.
 */
int SetDisplayMode(int width, int height, int bpp, int freq)
{
	// Step 2: set display mode using OpenGL friendly tactics

	DEVMODE devmode;
	devmode.dmSize = sizeof(DEVMODE);
	devmode.dmBitsPerPel = bpp;
	devmode.dmPelsWidth = width;
	devmode.dmPelsHeight = height;
	devmode.dmDisplayFlags = 0;
	devmode.dmDisplayFrequency = freq;
	devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFLAGS;
	if (freq != 0)
		devmode.dmFields |= DM_DISPLAYFREQUENCY;


	LONG cdsret = ChangeDisplaySettings(&devmode, CDS_FULLSCREEN);
	switch (cdsret) {
	case DISP_CHANGE_BADFLAGS :
		printf("Failed to set screen mode: bad flags\n");
		return -1;
	case DISP_CHANGE_FAILED:
		printf("Failed to set screen mode: change failed\n");
		return -1;
	case DISP_CHANGE_BADMODE:
		printf("Failed to set screen mode: bad mode\n");
		return -1;
	case DISP_CHANGE_SUCCESSFUL :
		// Success!
		break;
	default:
		printf("Failed to set screen mode: unknown error\n");
		return -1;
	}
	return 1;
}

/*
 * Class:     org_lwjgl_Display
 * Method:    getAvailableDisplayModes
 * Signature: ()[Lorg/lwjgl/DisplayMode;
 */
JNIEXPORT jobjectArray JNICALL Java_org_lwjgl_Display_getAvailableDisplayModes
  (JNIEnv * env, jclass clazz)
{

	DEVMODE mode;

	// First count the number of display modes.
	int i = 0, n = 0;
	while (EnumDisplaySettings(NULL, i ++, &mode) != 0) {
		// Filter out indexed modes
		if (mode.dmBitsPerPel < 16) {
			continue;
		} else
			n ++;
	}
		; // Do nothing

	// Allocate an array of DisplayModes big enough
	jclass displayModeClass = env->FindClass("org/lwjgl/DisplayMode");

	// Note the * 32 - this is because we are manufacturing available alpha/depth/stencil combos.
	jobjectArray ret = env->NewObjectArray(n * 16, displayModeClass, NULL);
	jmethodID displayModeConstructor = env->GetMethodID(displayModeClass, "<init>", "(IIIIIII)V");

	i = n = 0;
	while (EnumDisplaySettings(NULL, i ++, &mode) != 0) {
		// Filter out indexed modes
		if (mode.dmBitsPerPel <= 8) {
			continue;
		} else {
			jobject displayMode;

			for (int depthBits = 0; depthBits <= 24; depthBits += 8) {
				for (int stencilBits = 0; stencilBits <= 8; stencilBits += 8) {
					for (int alphaBits = 0; alphaBits <= 8; alphaBits += 8) {
			
						displayMode = env->NewObject(displayModeClass, displayModeConstructor, mode.dmPelsWidth, mode.dmPelsHeight,
							mode.dmBitsPerPel, mode.dmDisplayFrequency, alphaBits, depthBits, stencilBits);

						env->SetObjectArrayElement(ret, n ++, displayMode);
					}
				}
			}
		}
	}

	return ret;
}

/*
 * Class:     org_lwjgl_Display
 * Method:    nCreate
 * Signature: (IIIIZ)Z
 */
JNIEXPORT jboolean JNICALL Java_org_lwjgl_Display_nCreate
  (JNIEnv * env, jclass clazz, jint width, jint height, jint bpp, jint freq,
  jint alphaBits, jint depthBits, jint stencilBits, jboolean fullscreen)
{
#ifdef _DEBUG
	printf("Creating display: size %dx%d %dhz %dbpp...\n", width, height, freq, bpp);
#endif
	if (fullscreen && SetDisplayMode(width, height, bpp, freq) != 1)
		return JNI_FALSE;

	/*
		Register a window. This window does nothing, it's just a requirement that we get
		a handle to it so we can do other things
	*/
	if (!oneShotInitialised) {
		WNDCLASS windowClass;

		windowClass.style = CS_GLOBALCLASS | CS_OWNDC;
		windowClass.lpfnWndProc = WindowProc;
		windowClass.cbClsExtra = 0;
		windowClass.cbWndExtra = 0;
		windowClass.hInstance = dll_handle;
		windowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		windowClass.hCursor = LoadIcon(NULL, IDC_ARROW);
		windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		windowClass.lpszMenuName = NULL;
		windowClass.lpszClassName = WINDOWCLASSNAME;

		if (RegisterClass(&windowClass) == 0) {
			dumpLastError();
			printf("Failed to register window class\n");
			return JNI_FALSE;
		}
		oneShotInitialised = true;
	}

	int windowflags;

	if (fullscreen) {
		windowflags = WS_POPUP;
	} else {
		windowflags = WS_POPUP | WS_CAPTION;
	}

	// Create the window now, using that class:
	hwnd = CreateWindow(
		WINDOWCLASSNAME,
		"LWJGL",
		windowflags,
		0, 0,
		width, height,
		NULL,
		NULL,
		dll_handle,
		NULL);
	// And we never look at windowClass again...

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	hdc = GetWindowDC(hwnd);  

	// Success! Now you need to initialize a GL object, which creates a GL rendering context;
	// and then to issue commands to it, you need to call gl::makeCurrent().
#ifdef _DEBUG
	printf("Created display\n");
#endif

	// Hide the mouse
	ShowCursor(FALSE);

	// Create input
	HRESULT ret = DirectInputCreate(GetModuleHandle(NULL), DIRECTINPUT_VERSION, &lpdi, NULL);
	if (ret != DI_OK && ret != DIERR_BETADIRECTINPUTVERSION ) {
		printf("Failed to create directinput");
		switch (ret) {
			case DIERR_BETADIRECTINPUTVERSION :
				printf(" - Beta version\n");
				break;
			case DIERR_INVALIDPARAM :
				printf(" - Invalid parameter\n");
				break;
			case DIERR_OLDDIRECTINPUTVERSION :
				printf(" - Old Version\n");
				break;
			case DIERR_OUTOFMEMORY :
				printf(" - Out Of Memory\n");
				break;
			default:
				printf("\n");
		}
		destroyWindow();
		return JNI_FALSE;
	}

	int flags = PFD_DRAW_TO_WINDOW |   // support window 
		PFD_SUPPORT_OPENGL |   // support OpenGL 
		PFD_GENERIC_ACCELERATED |
		PFD_DOUBLEBUFFER;      // double buffered 

	PIXELFORMATDESCRIPTOR pfd = { 
		sizeof(PIXELFORMATDESCRIPTOR),   // size of this pfd 
		1,                     // version number 
		flags,         // RGBA type 
		PFD_TYPE_RGBA,
		(BYTE)bpp,       
		0, 0, 0, 0, 0, 0,      // color bits ignored 
		(BYTE)alphaBits,       
		0,                     // shift bit ignored 
		0,                     // no accumulation buffer 
		0, 0, 0, 0,            // accum bits ignored 
		(BYTE)depthBits,       
		(BYTE)stencilBits,     
		0,                     // No auxiliary buffer 
		PFD_MAIN_PLANE,        // main layer
		0,                     // reserved 
		0, 0, 0                // layer masks ignored
	};

	// Ensure desktop color depth is adequate
	int availableBitDepth = GetDeviceCaps(hdc, BITSPIXEL);
	if (availableBitDepth < bpp) {
		printf("This application requires a greater colour depth.\n");
		destroyAll();
		return JNI_FALSE;
	};

	int  iPixelFormat;  

	// get the best available match of pixel format for the device context  
	iPixelFormat = ChoosePixelFormat(hdc, &pfd);
	if (iPixelFormat == 0) {
		printf("Failed to choose pixel format.\n");
		destroyAll();
		return JNI_FALSE;
	}

	PIXELFORMATDESCRIPTOR desc;
	if (DescribePixelFormat(hdc, iPixelFormat, sizeof(PIXELFORMATDESCRIPTOR), &desc) == 0) {
		printf("Could not describe pixel format\n");
		destroyAll();
		return JNI_FALSE;
	}

	if (desc.cColorBits < bpp) {
		printf("This application requires a greater colour depth.\n");
		destroyAll();
		return JNI_FALSE;
	}

	if (desc.cStencilBits < stencilBits) {
		printf("This application requires a greater stencil depth.\n");
		destroyAll();
		return JNI_FALSE;
	}

	if (desc.cDepthBits < depthBits) {
		printf("This application requires a greater depth buffer depth.\n");
		destroyAll();
		return JNI_FALSE;
	}

	if ((desc.dwFlags & flags) == 0) {
		printf("Capabilities not supported.\n");
		destroyAll();
		return JNI_FALSE;
	}

#ifdef _DEBUG
	printf("Pixel format is %d\n", iPixelFormat);
#endif

	// make that the pixel format of the device context 
	if (SetPixelFormat(hdc, iPixelFormat, &pfd) == FALSE) {
		printf("Failed to set pixel format\n");
		destroyAll();
		return JNI_FALSE;
	}

	jfieldID fid_handle = env->GetStaticFieldID(clazz, "handle", "I");
	env->SetStaticIntField(clazz, fid_handle, (jint) hwnd);

	return JNI_TRUE;
}

/*
 * Class:     org_lwjgl_Display
 * Method:    nDestroy
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_lwjgl_Display_nDestroy
  (JNIEnv * env, jclass clazz)
{
	destroyAll();
}

