/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef VERSION_H
#define VERSION_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gbax-util/dllexports.h>

extern GBAX_EXPORT const char* const gitCommit;
extern GBAX_EXPORT const char* const gitCommitShort;
extern GBAX_EXPORT const char* const gitBranch;
extern GBAX_EXPORT const int gitRevision;
extern GBAX_EXPORT const char* const binaryName;
extern GBAX_EXPORT const char* const projectName;
extern GBAX_EXPORT const char* const projectVersion;

#ifdef __cplusplus
}
#endif

#endif
