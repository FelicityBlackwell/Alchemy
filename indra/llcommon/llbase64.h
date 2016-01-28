/** 
 * @file llbase64.h
 * @brief Wrapper for apr base64 encoding that returns a std::string
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LLBASE64_H
#define LLBASE64_H

class LL_COMMON_API LLBase64
{
public:
	static std::string encode(const std::string& in_str);
	static std::string encode(const U8* input, size_t input_size);
	static size_t decode(const std::string& input, U8 * buffer, size_t buffer_size);
	static std::string decode(const std::string& input);
	static size_t requiredDecryptionSpace(const std::string& str);
	
	// *HACK: Special case because of OpenSSL ASCII/EBCDIC issues
	static size_t apr_base64_decode_binary(U8 *bufplain, const char* bufcoded);
};

#endif
