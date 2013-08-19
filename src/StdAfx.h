/* Shroud_Plugin - for licensing and copyright see license.txt */

#pragma once

/* #undef _HAS_ITERATOR_DEBUGGING
#define _HAS_ITERATOR_DEBUGGING 0
#undef _SECURE_SCL
#define _SECURE_SCL 0
#undef _SECURE_SCL_THROWS
#define _SECURE_SCL_THROWS 0 */

#ifdef _HAS_ITERATOR_DEBUGGING
#endif

// Insert your headers here
#include <platform.h>
#include <algorithm>
#include <vector>
#include <memory>
#include <list>
#include <functional>
#include <limits>
#include <smartptr.h>
#include <CryThread.h>
#include <Cry_Math.h>
#include <ISystem.h>
#include <I3DEngine.h>
#include <IInput.h>
#include <IConsole.h>
#include <ITimer.h>
#include <ILog.h>
#include <IGameplayRecorder.h>
#include <ISerialize.h>
#include <IRenderMesh.h>
#include <IIndexedMesh.h>
#include <IViewSystem.h>
#include <ICryAnimation.h>
#include <IEntitySystem.h>
#include <IEntityRenderState.h>

#include <ICryPak.h>
#define fread gEnv->pCryPak->FReadRaw
#define fopen gEnv->pCryPak->FOpen
#define fclose gEnv->pCryPak->FClose
#define fseek gEnv->pCryPak->FSeek
#define ftell gEnv->pCryPak->FTell
#define feof gEnv->pCryPak->FEof
#define ferror gEnv->pCryPak->FError
#define rewind(stream) gEnv->pCryPak->FSeek(stream, 0L, SEEK_SET)

#include <Shroud/Shroud.h>

#ifndef _FORCEDLL
#define _FORCEDLL
#endif

#ifndef SHROUDPLUGIN_EXPORTS
#define SHROUDPLUGIN_EXPORTS
#endif

#pragma warning(disable: 4018)  // conditional expression is constant

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.
