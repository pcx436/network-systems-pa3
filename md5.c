/**
 * This implementation of MD5 was directly sourced from the following GitHub repository: https://github.com/pod32g/MD5
 * I take no credit for this program.
 * Copied on November 11th, 2020.
 */

/*
 * Simple MD5 implementation
 *
 * Compile with: gcc -o md5 md5.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "md5.h"
#include "macro.h"

void to_bytes(uint32_t val, uint8_t *bytes) {
	bytes[0] = (uint8_t) val;
	bytes[1] = (uint8_t) (val >> 8);
	bytes[2] = (uint8_t) (val >> 16);
	bytes[3] = (uint8_t) (val >> 24);
}

uint32_t to_int32(const uint8_t *bytes) {
	return (uint32_t) bytes[0]
	       | ((uint32_t) bytes[1] << 8)
	       | ((uint32_t) bytes[2] << 16)
	       | ((uint32_t) bytes[3] << 24);
}

void md5(const uint8_t *initial_msg, size_t initial_len, uint8_t *digest) {

	// These vars will contain the hash
	uint32_t h0, h1, h2, h3;

	// Message (to prepare)
	uint8_t *msg = NULL;

	size_t new_len, offset;
	uint32_t w[16];
	uint32_t a, b, c, d, i, f, g, temp;

	// Initialize variables - simple count in nibbles:
	h0 = 0x67452301;
	h1 = 0xefcdab89;
	h2 = 0x98badcfe;
	h3 = 0x10325476;

	//Pre-processing:
	//append "1" bit to message
	//append "0" bits until message length in bits ≡ 448 (mod 512)
	//append length mod (2^64) to message

	for (new_len = initial_len + 1; new_len % (512 / 8) != 448 / 8; new_len++);

	msg = (uint8_t *) malloc(new_len + 8);
	memcpy(msg, initial_msg, initial_len);
	msg[initial_len] = 0x80; // append the "1" bit; most significant bit is "first"
	for (offset = initial_len + 1; offset < new_len; offset++)
		msg[offset] = 0; // append "0" bits

	// append the len in bits at the end of the buffer.
	to_bytes(initial_len * 8, msg + new_len);
	// initial_len>>29 == initial_len*8>>32, but avoids overflow.
	to_bytes(initial_len >> 29, msg + new_len + 4);

	// Process the message in successive 512-bit chunks:
	//for each 512-bit chunk of message:
	for (offset = 0; offset < new_len; offset += (512 / 8)) {

		// break chunk into sixteen 32-bit words w[j], 0 ≤ j ≤ 15
		for (i = 0; i < 16; i++)
			w[i] = to_int32(msg + offset + i * 4);

		// Initialize hash value for this chunk:
		a = h0;
		b = h1;
		c = h2;
		d = h3;

		// Main loop:
		for (i = 0; i < 64; i++) {

			if (i < 16) {
				f = (b & c) | ((~b) & d);
				g = i;
			} else if (i < 32) {
				f = (d & b) | ((~d) & c);
				g = (5 * i + 1) % 16;
			} else if (i < 48) {
				f = b ^ c ^ d;
				g = (3 * i + 5) % 16;
			} else {
				f = c ^ (b | (~d));
				g = (7 * i) % 16;
			}

			temp = d;
			d = c;
			c = b;
			b = b + LEFTROTATE((a + f + k[i] + w[g]), r[i]);
			a = temp;

		}

		// Add this chunk's hash to result so far:
		h0 += a;
		h1 += b;
		h2 += c;
		h3 += d;

	}

	// cleanup
	free(msg);

	//var char digest[16] := h0 append h1 append h2 append h3 //(Output is in little-endian)
	to_bytes(h0, digest);
	to_bytes(h1, digest + 4);
	to_bytes(h2, digest + 8);
	to_bytes(h3, digest + 12);
}

// The following was coded by Jacob Malcy
char * md5Str(char *msg, char *strResult) {
	// error check
	if (msg == NULL || (msg != NULL && strlen(msg) == 0))
		return NULL;

	int i;
	uint8_t uintResult[16];
	size_t len = strlen(msg);
	char *charCast;

	bzero(strResult, HEX_BYTES + 1);  // clear out the result array

	md5((uint8_t *) msg, len, uintResult);

	// build result
	for (i = 0; i < 16; i++) {
		charCast = malloc(3);  // 2 hex + \0
		bzero(charCast, 3);
		snprintf(charCast, 3, "%2.2x", uintResult[i]);
		strcat(strResult, charCast);
		free(charCast);
	}
	return strResult;
}
