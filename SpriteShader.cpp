/* SpriteShader.cpp
Copyright (c) 2014 by Michael Zahniser

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "SpriteShader.h"

#include "Point.h"
#include "Screen.h"
#include "Shader.h"
#include "Sprite.h"

#include <cmath>

using namespace std;

namespace {
	Shader shader;
	GLint scaleI;
	GLint transformI;
	GLint positionI;
	GLint clipI;
	GLint fadeI;
	
	GLuint vao;
	GLuint vbo;

	static const GLint SWIZZLE[9][4] = {
		{GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA}, // red + yellow markings (republic)
		{GL_RED, GL_BLUE, GL_GREEN, GL_ALPHA}, // red + magenta markings
		{GL_GREEN, GL_RED, GL_BLUE, GL_ALPHA}, // green + yellow (freeholders)
		{GL_BLUE, GL_RED, GL_GREEN, GL_ALPHA}, // green + cyan
		{GL_GREEN, GL_BLUE, GL_RED, GL_ALPHA}, // blue + magenta (syndicate)
		{GL_BLUE, GL_GREEN, GL_RED, GL_ALPHA}, // blue + cyan (merchant)
		{GL_GREEN, GL_BLUE, GL_BLUE, GL_ALPHA}, // red and black (pirate)
		{GL_BLUE, GL_ZERO, GL_ZERO, GL_ALPHA},  // red only (cloaked)
		{GL_ZERO, GL_ZERO, GL_ZERO, GL_ALPHA}  // black only (outline)
	};
}



// Initialize the shaders.
void SpriteShader::Init()
{
	static const char *vertexCode =
		"uniform mat2 transform;\n"
		"uniform vec2 position;\n"
		"uniform vec2 scale;\n"
		"uniform float clip;\n"
		
		"in vec2 vert;\n"
		"out vec2 fragTexCoord;\n"
		
		"void main() {\n"
		"  gl_Position = vec4((transform * vert + position) * scale, 0, 1);\n"
		"  vec2 texCoord = vert + vec2(.5, .5);\n"
		"  fragTexCoord = vec2(texCoord.x, max(clip, texCoord.y));\n"
		"}\n";

	static const char *fragmentCode =
		"uniform sampler2D tex0;\n"
		"uniform sampler2D tex1;\n"
		"uniform float fade;\n"
		
		"in vec2 fragTexCoord;\n"
		"out vec4 finalColor;\n"
		
		"void main() {\n"
		// Since fade is a uniform, this branch will not cause performance trouble.
		"  if(fade != 0)\n"
		"    finalColor = mix(texture(tex0, fragTexCoord), texture(tex1, fragTexCoord), fade);\n"
		"  else\n"
		"    finalColor = texture(tex0, fragTexCoord);\n"
		"}\n";
	
	shader = Shader(vertexCode, fragmentCode);
	scaleI = shader.Uniform("scale");
	transformI = shader.Uniform("transform");
	positionI = shader.Uniform("position");
	clipI = shader.Uniform("clip");
	fadeI = shader.Uniform("fade");
	
	glUseProgram(shader.Object());
	glUniform1i(shader.Uniform("tex0"), 0);
	glUniform1i(shader.Uniform("tex1"), 1);
	glUseProgram(0);
	
	// Generate the vertex data for drawing sprites.
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	
	GLfloat vertexData[] = {
		-.5f, -.5f,
		-.5f,  .5f,
		 .5f, -.5f,
		 .5f,  .5f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertexData), vertexData, GL_STATIC_DRAW);
	
	glEnableVertexAttribArray(shader.Attrib("vert"));
	glVertexAttribPointer(shader.Attrib("vert"), 2, GL_FLOAT, GL_FALSE,
		2 * sizeof(GLfloat), NULL);
	
	// unbind the VBO and VAO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
}



void SpriteShader::Draw(const Sprite *sprite, const Point &position, float zoom, int swizzle)
{
	if(!sprite)
		return;
	
	float pos[2] = {
		static_cast<float>(position.X()), static_cast<float>(position.Y())};
	float trans[4] = {sprite->Width() * zoom, 0.f, 0.f, sprite->Height() * zoom};
	
	Bind();
	Add(sprite->Texture(), 0, pos, trans, swizzle, 1.f, 0.f);
	Unbind();
}



void SpriteShader::Bind()
{
	glUseProgram(shader.Object());
	glBindVertexArray(vao);
	glActiveTexture(GL_TEXTURE0);
	
	GLfloat scale[2] = {2.f / Screen::Width(), -2.f / Screen::Height()};
	glUniform2fv(scaleI, 1, scale);
}



void SpriteShader::Add(uint32_t tex0, uint32_t tex1, const float position[2], const float transform[4], int swizzle, float clip, float fade)
{
	glUniformMatrix2fv(transformI, 1, false, transform);
	glUniform2fv(positionI, 1, position);
	glBindTexture(GL_TEXTURE_2D, tex0);
	
	if(fade && tex1)
	{
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, tex1);
		glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, SWIZZLE[swizzle]);
		glActiveTexture(GL_TEXTURE0);
	}
	else
		fade = 0.f;
	
	// Set the color swizzle.
	glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, SWIZZLE[swizzle]);
	
	// Set the clipping.
	glUniform1f(clipI, 1.f - clip);
	glUniform1f(fadeI, fade);
	
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}



void SpriteShader::Unbind()
{
	glBindVertexArray(0);
	glUseProgram(0);
}
