	
/**
 * Convert the specified RGBA data buffer into the RGB5A3 texture format
 * 
 * This routine converts the RGBA data buffer into the RGB5A3 texture format and returns a pointer to the converted buffer.
 * 
 * @param rgbaBuffer	Buffer containing the temporarily rendered RGBA data.
 * @param bufferWidth	Pixel width of the data buffer.
 * @param bufferHeight	Pixel height of the data buffer.
 * @return	A pointer to the allocated buffer.
 */

#include "metaphrasis.h"
#include "os.h"

uint32_t* Metaphrasis_convertBufferToRGB5A3(uint32_t *rgbaBuffer, uint32_t *outBuffer, uint16_t bufferWidth, uint16_t bufferHeight) {
	uint32_t bufferSize = (bufferWidth * bufferHeight) << 1;
	uint32_t* dataBufferRGB5A3 = (uint32_t *)outBuffer;

	uint32_t *src = (uint32_t *)rgbaBuffer;
	uint16_t *dst = (uint16_t *)dataBufferRGB5A3;

	for(uint16_t y = 0; y < bufferHeight; y += 4) {
		for(uint16_t x = 0; x < bufferWidth; x += 4) {
			for(uint16_t rows = 0; rows < 4; rows++) {
				*dst++ = RGBA_TO_RGB5A3(src[((y + rows) * bufferWidth) + (x + 0)]);
				*dst++ = RGBA_TO_RGB5A3(src[((y + rows) * bufferWidth) + (x + 1)]);
				*dst++ = RGBA_TO_RGB5A3(src[((y + rows) * bufferWidth) + (x + 2)]);
				*dst++ = RGBA_TO_RGB5A3(src[((y + rows) * bufferWidth) + (x + 3)]);
			}
		}
	}
	DCFlushRange(dataBufferRGB5A3, bufferSize);

	return dataBufferRGB5A3;
}
