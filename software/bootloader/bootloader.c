// Written by:
//     Evan Andersen
//
// Created January 21, 2017

#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "gpu_driver.h"
#include "ext2_driver.h"
#include "system.h"

#define PI (3.14159265359f)

const color_t colors[] = {
	COLOR(13,20,31),
	COLOR(31,31,0),
	COLOR(31,0,0),
	COLOR(0,29,5),
	COLOR(15,0,19),
	COLOR(31,16,0),
};

size_t tokenizeString(char *src, char **tokens, size_t size) {
	size_t n = 0;
	while(n < size && *src != '\0') {
		//consume whitespace
		while(isspace(*src)) {
			src++;
			if(*src == '\0') {
				return n;
			}
		}
		tokens[n] = src;
		n++;
		//consume non-whitespace
		while(!isspace(*src)) {
			src++;
			if(*src == '\0') {
				return n;
			}
		}
		*src = '\0';
		src++;
	}
	return n;
}

void getNextLine(char *src, size_t *pos, size_t size, char *dst, size_t n) {
	size_t copied = 0;
	while(*pos < size) {
		char next = src[*pos];
		(*pos)++;

		if(next == '\n') {
			break;
		} else {
			dst[copied] = next;
			copied++; 
		}
	}
	dst[copied] = '\0';
}

int main(void) {
	int result = 1;
	while(result != 0) {
		result = ext2_init();
		*LEDR = result;
	}
	struct inode testInode;
	ext2_findRootFile("test.txt", &testInode);

	//alloc some scratch for the file contents
	char *buf = alloc_tmp(testInode.sizel);
	ext2_readall(&testInode, buf);
	*LEDR = buf[testInode.sizel-2];

	//instead of counting just guess that it's <2000
	point_t *vertices = alloc_perm(sizeof(point_t) * 2000);
    uint16_t nvertices = 0;

	//instead of counting just guess that it's <1000
	triangle_t *tris = alloc_perm(sizeof(triangle_t) * 1000);
    uint16_t ntris = 0;

    //parse the file
    size_t pos = 0;
    while(pos < testInode.sizel) {
    	char lineBuf[128];
    	getNextLine(buf, &pos, testInode.sizel, lineBuf, sizeof(lineBuf));
    	char *tokens[10];
    	int nTok = tokenizeString(lineBuf, tokens, sizeof(tokens)/sizeof(tokens[0]));
    	//both triangles and vertices have 4 tokens
    	if(nTok == 4) {
    		if(!strcmp(tokens[0], "v")){
    			//found a vertex
    			vertices[nvertices].x = strtof(tokens[1], NULL);
	    		vertices[nvertices].y = strtof(tokens[2], NULL);
	    		vertices[nvertices].z = strtof(tokens[3], NULL);
	    		nvertices++;
    		} else if(!strcmp(tokens[0], "f")) {
    			//found a triangle
    			tris[ntris].v0 = strtol(tokens[1], NULL, 10) - 1;
	    		tris[ntris].v1 = strtol(tokens[2], NULL, 10) - 1;
	    		tris[ntris].v2 = strtol(tokens[3], NULL, 10) - 1;
	    		tris[ntris].color = colors[ntris % (sizeof(colors)/sizeof(colors[0]))];
	    		ntris++;
    		}
    	}
    }

	uint16_t *frontFB = alloc_perm(800*600*sizeof(uint16_t));
	uint16_t *backFB = alloc_perm(800*600*sizeof(uint16_t));

    	
	VGA[1] = (uint32_t)frontFB;
	
	int32_t xpos = 0;
	int32_t ypos = 0;
	int32_t zpos = 0;
    
    uint32_t frame = 0;
	while(1) {

		if(*KEY & 0x1) {
			xpos += 5;
			xpos %= 360;
		}
		if(*KEY & 0x2) {
			ypos += 5;
			ypos %= 360;
		}
		if(*KEY & 0x4) {
			zpos += 5;
			zpos %= 360;
		}
				        
		render_target_t target = {
			.framebuffer = backFB,
			.stride = 800*2,
			.width = 800,
			.height = 600,
		};
				
		float xAngle = (xpos/360.0f)*PI*2;
		float sinX = sin(xAngle);
		float cosX = cos(xAngle);
		float rotMatX[] = {
			1, 0, 0, 0,
			0, cosX, -sinX, 0,
			0, sinX, cosX, 0,
			0, 0, 0, 1,
		};
		float yAngle = (ypos/360.0f)*PI*2;
		float sinY = sin(yAngle);
		float cosY = cos(yAngle);
		float rotMatY[] = {
			cosY, 0, sinY, 0,
			0, 1, 0, 0,
			-sinY, 0, cosY, 0,
			0, 0, 0, 1,
		};
		float zAngle = (zpos/360.0f)*PI*2;
		float sinZ = sin(zAngle);
		float cosZ = cos(zAngle);
		float rotMatZ[] = {
			cosZ, -sinZ, 0, 0,
			sinZ, cosZ, 0, 0,
			0, 0, 1, 0,
			0, 0, 0, 1,
		};

		float viewMat[] = {
			0.02, 0, 0, 0,
			0, 0.02, 0, 0,
			0, 0, 0.02, -4.0,
			0, 0, 0, 1,			
		};
		
		float near = 0.1;
		float far = 100;
		float top = 0.1;
		float bottom = -top;
		float right = (4.0f/3.0f) * top;
		float left = -right;
		
		float tmp[16];
		scene_t scene = {
			.poly_list = {
				.vertices = vertices,
				.triangles = tris,
				.nvertices = nvertices,
				.ntris = ntris,
			},
			//perspective
			.proj = {
				2*near/(right-left), 0, (right+left)/(right-left), 0,
				0, 2*near/(top-bottom), (top+bottom)/(top-bottom), 0,
				0, 0, -(far+near)/(far-near), -(2*far*near)/(far-near),
				0, 0, -1, 0,			
			},
			
			//ortho
			// .proj = {
				// 2/(right-left), 0, 0, -(right+left)/(right-left),
				// 0, 2/(top-bottom), 0, -(top+bottom)/(top-bottom),
				// 0, 0, -2/(far-near), -(far+near)/(far-near),
				// 0, 0, 0, 1,			
			// },
		};
		mult_4x4(rotMatY, rotMatX, scene.view);
		mult_4x4(rotMatZ, scene.view, tmp);
		mult_4x4(viewMat, tmp, scene.view);

		//if(SW[0] & 0x2) {
		//	gradient(frontFB);
		//	flush_dcache();
		//	while(SW[0] & 0x2);
		//}
		//if(SW[0] & 0x4) {
		//	draw_checkerboard_gpu(frontFB);
		//	while(SW[0] & 0x4);
		//}
		
		//time for the magic
		gpu_render_scene(&target, &scene);
				
		//send the fresh frame to the VGA module
		VGA[1] = (uint32_t)backFB;
		
		//wait for the VGA module to start using the new buffer
		while(VGA[1] != (uint32_t)backFB);
		
		//how many times did the screen draw while we rendered this frame?
        uint32_t frames = VGA[0];
		//*LEDR = frames;
        frame += frames;
		
		//swap back/front buffers 
		uint16_t *tmpFB = backFB;
		backFB = frontFB;
		frontFB = tmpFB;
	}
	return 0;
}
