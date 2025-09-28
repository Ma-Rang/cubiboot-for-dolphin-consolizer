/* 
 * Metaphrasis is a static conversion class for transforming RGBA image
 * buffers into verious GX texture formats for Wii homebrew development.
 * Copyright (C) 2008 Armin Tamzarian
 * 
 * This file is part of Metaphrasis.
 * 
 * Metaphrasis is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Metaphrasis is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with Metaphrasis.  If not, see <http://www.gnu.org/licenses/>.
 */

/** \mainpage Metaphrasis
 *
 * \section sec_intro Introduction
 * 
 * Metaphrasis is a static conversion class for transforming RGBA image buffers into verious GX texture formats for Wii homebrew development.
 * <br>
 * Metaphrasis is written in C++ and makes use of a community standard and newly developed algorithms for conversion of 32-bit RGBA data buffers into various GX texture formats common to both the Gamecube and Wii platforms.  
 * <p>
 * This library was developed in-full by Armin Tamzarian with the support of developers in \#wiidev on EFnet, Chaosteil of libwiisprite, and DrTwox of GRRLIB.
 */

#include <gctypes.h>
#include <stdint.h>

/*! \class Metaphrasis
 * \brief A static conversion class for transforming RGBA image buffers into verious GX texture formats for
 * Wii homebrew development.
 * \author Armin Tamzarian
 * \version 0.1.2
 */

#define RGBA_TO_RGB555(x) ((x & 0xf8000000) >> 17) | ((x & 0x00f80000) >> 14) | ((x & 0x0000f800) >> 11) | 0x8000
#define RGBA_TO_RGB444(x) ((x & 0xf0000000) >> 17) | ((x & 0x00f00000) >> 13) | ((x & 0x0000f000) >> 9) | ((x & 0x000000e0) >> 5)
#define RGBA_TO_RGB5A3(x) (x & 0xff) < 0xe0 ? RGBA_TO_RGB444(x) : RGBA_TO_RGB555(x)

uint32_t* Metaphrasis_convertBufferToRGBA8(uint32_t* rgbaBuffer, uint32_t *outBuffer, uint16_t bufferWidth, uint16_t bufferHeight);
uint32_t* Metaphrasis_convertBufferToRGB5A3(uint32_t *rgbaBuffer, uint32_t *outBuffer, uint16_t bufferWidth, uint16_t bufferHeight);
