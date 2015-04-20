/*
 * Copyright 2014 SpeechTech, s.r.o. http://www.speechtech.cz/en
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * $Id$
 */

/**
 * @file UniMRCP-wrapper-version.h
 * @brief UniMRCP Client wrapper version declaration.
 */

#ifndef UNIMRCP_WRAPPER_VERSION_H
#define UNIMRCP_WRAPPER_VERSION_H

#define UW_MAJOR_VERSION 0
#define UW_MINOR_VERSION 4
#define UW_PATCH_VERSION 5

#define UW_STRINGIFY(s)  UW_STRINGIFY_(s)
#define UW_STRINGIFY_(s) #s

#define UW_VERSION_STRING \
	UW_STRINGIFY(UW_MAJOR_VERSION) "." \
	UW_STRINGIFY(UW_MINOR_VERSION) "." \
	UW_STRINGIFY(UW_PATCH_VERSION)

#define UW_VERSION_CSV \
	UW_MAJOR_VERSION,  \
	UW_MINOR_VERSION,  \
	UW_PATCH_VERSION

#define UW_LICENSE \
	"Licensed under the Apache License, Version 2.0 (the ""License"");\n" \
	"you may not use this file except in compliance with the License.\n" \
	"You may obtain a copy of the License at\n" \
	"\n" \
	"    http://www.apache.org/licenses/LICENSE-2.0\n" \
	"\n" \
	"Unless required by applicable law or agreed to in writing, software\n" \
	"distributed under the License is distributed on an ""AS IS"" BASIS,\n" \
	"WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n" \
	"See the License for the specific language governing permissions and\n" \
	"limitations under the License.\n"

#endif  /* UNIMRCP_WRAPPER_VERSION_H */
