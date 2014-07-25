// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#include "VideoRenderer.h"
#include "VideoDecoder.h"
#include "QuickRouteJpegReader.h"

using namespace OrientView;

VideoRenderer::VideoRenderer()
{
}

bool VideoRenderer::initialize(VideoDecoder* videoDecoder, QuickRouteJpegReader* quickRouteJpegReader)
{
	qDebug("Initializing VideoRenderer");

	this->videoFrameWidth = videoDecoder->getFrameWidth();
	this->videoFrameHeight = videoDecoder->getFrameHeight();
	this->mapImageWidth = quickRouteJpegReader->getMapImage().width();
	this->mapImageHeight = quickRouteJpegReader->getMapImage().height();

	initializeOpenGLFunctions();

	qDebug("Compiling shaders");

	shaderProgram = std::unique_ptr<QOpenGLShaderProgram>(new QOpenGLShaderProgram());

	if (!shaderProgram->addShaderFromSourceFile(QOpenGLShader::Vertex, "data/shaders/basic.vert"))
		return false;

	if (!shaderProgram->addShaderFromSourceFile(QOpenGLShader::Fragment, "data/shaders/basic.frag"))
		return false;

	if (!shaderProgram->link())
		return false;

	if ((vertexMatrixUniform = shaderProgram->uniformLocation("vertexMatrix")) == -1)
	{
		qWarning("Could not find vertexMatrix uniform");
		return false;
	}

	if ((textureMatrixUniform = shaderProgram->uniformLocation("textureMatrix")) == -1)
	{
		qWarning("Could not find textureMatrix uniform");
		return false;
	}

	if ((textureSamplerUniform = shaderProgram->uniformLocation("textureSampler")) == -1)
	{
		qWarning("Could not find textureSampler uniform");
		return false;
	}

	if ((vertexCoordAttribute = shaderProgram->attributeLocation("vertexCoord")) == -1)
	{
		qWarning("Could not find vertexCoord attribute");
		return false;
	}

	if ((textureCoordAttribute = shaderProgram->attributeLocation("textureCoord")) == -1)
	{
		qWarning("Could not find textureCoord attribute");
		return false;
	}

	videoPanelBuffer = std::unique_ptr<QOpenGLBuffer>(new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer));
	videoPanelBuffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
	videoPanelBuffer->create();
	videoPanelBuffer->bind();
	videoPanelBuffer->allocate(sizeof(GLfloat) * 16);
	videoPanelBuffer->release();

	videoPanelTexture = std::unique_ptr<QOpenGLTexture>(new QOpenGLTexture(QOpenGLTexture::Target2D));
	videoPanelTexture->create();
	videoPanelTexture->bind();
	videoPanelTexture->setSize(videoFrameWidth, videoFrameHeight);
	videoPanelTexture->setFormat(QOpenGLTexture::RGBA8_UNorm);
	videoPanelTexture->setMinificationFilter(QOpenGLTexture::Linear);
	videoPanelTexture->setMagnificationFilter(QOpenGLTexture::Linear);
	videoPanelTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
	videoPanelTexture->allocateStorage();
	videoPanelTexture->release();

	mapPanelBuffer = std::unique_ptr<QOpenGLBuffer>(new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer));
	mapPanelBuffer->setUsagePattern(QOpenGLBuffer::DynamicDraw);
	mapPanelBuffer->create();
	mapPanelBuffer->bind();
	mapPanelBuffer->allocate(sizeof(GLfloat) * 16);
	mapPanelBuffer->release();

	mapPanelTexture = std::unique_ptr<QOpenGLTexture>(new QOpenGLTexture(quickRouteJpegReader->getMapImage()));
	mapPanelTexture->bind();
	mapPanelTexture->setMinificationFilter(QOpenGLTexture::Linear);
	mapPanelTexture->setMagnificationFilter(QOpenGLTexture::Linear);
	mapPanelTexture->setBorderColor(1.0f, 1.0f, 1.0f, 1.0f);
	mapPanelTexture->setWrapMode(QOpenGLTexture::ClampToBorder);
	mapPanelTexture->release();

	return true;
}

void VideoRenderer::shutdown()
{
	qDebug("Shutting down VideoRenderer");

	if (videoPanelBuffer != nullptr)
		videoPanelBuffer.reset(nullptr);

	if (videoPanelTexture != nullptr)
		videoPanelTexture.reset(nullptr);

	if (mapPanelBuffer != nullptr)
		mapPanelBuffer.reset(nullptr);

	if (mapPanelTexture != nullptr)
		mapPanelTexture.reset(nullptr);

	if (shaderProgram != nullptr)
		shaderProgram.reset(nullptr);
}

void VideoRenderer::update(int windowWidth, int windowHeight)
{
	float mapPanelWidth = 0.2f;
	float mapPanelWidthInverse = 1.0f - mapPanelWidth;
	float scaledWindowWidth = mapPanelWidthInverse * windowWidth;

	float videoPanelTop = 1.0f;
	float videoPanelBottom = 0.0f;
	float videoPanelLeft = mapPanelWidth;
	float videoPanelRight = 1.0f;

	// try to fit the video panel on the screen as big as possible and keep the video aspect ratio
	float videoAspectRatio = (float)videoFrameWidth / videoFrameHeight;
	float newVideoWidth = windowHeight * videoAspectRatio;
	float newVideoHeight = scaledWindowWidth / videoAspectRatio;

	// scale horizontally
	if (newVideoWidth < scaledWindowWidth)
		videoPanelLeft = mapPanelWidth + ((1.0f - (newVideoWidth / scaledWindowWidth)) * mapPanelWidthInverse);

	// scale vertically
	if (newVideoHeight < windowHeight)
		videoPanelBottom = 1.0f - (newVideoHeight / windowHeight);

	// center horizontally
	float halfLeft = (videoPanelLeft - mapPanelWidth) / 2.0f;
	videoPanelLeft -= halfLeft;
	videoPanelRight -= halfLeft;

	// center vertically
	float halfFromBottom = videoPanelBottom / 2.0f;
	videoPanelBottom -= halfFromBottom;
	videoPanelTop -= halfFromBottom;

	GLfloat videoPanelBufferData[] =
	{
		videoPanelLeft, videoPanelBottom,
		videoPanelRight, videoPanelBottom,
		videoPanelRight, videoPanelTop,
		videoPanelLeft, videoPanelTop,

		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f
	};

	videoPanelBuffer->bind();
	videoPanelBuffer->write(0, videoPanelBufferData, sizeof(GLfloat) * 16);
	videoPanelBuffer->release();

	float mapAspectRatio = (float)mapImageWidth / mapImageHeight;
	float mapPanelTextureRight = (mapPanelWidth * windowWidth) / mapImageWidth;
	float mapPanelTextureTop = windowHeight / (float)mapImageHeight;

	GLfloat mapPanelBufferData[] =
	{
		0.0f, 0.0f,
		mapPanelWidth, 0.0f,
		mapPanelWidth, 1.0f,
		0.0f, 1.0f,

		0.0f, mapPanelTextureTop,
		mapPanelTextureRight, mapPanelTextureTop,
		mapPanelTextureRight, 0.0f,
		0.0f, 0.0f
	};

	mapPanelBuffer->bind();
	mapPanelBuffer->write(0, mapPanelBufferData, sizeof(GLfloat) * 16);
	mapPanelBuffer->release();

	videoVertexMatrix.setToIdentity();
	videoTextureMatrix.setToIdentity();
	mapVertexMatrix.setToIdentity();
	mapTextureMatrix.setToIdentity();

	videoVertexMatrix.ortho(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	mapVertexMatrix.ortho(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f);

	mapTextureMatrix.scale(5.0f);
}

void VideoRenderer::render()
{
	shaderProgram->bind();
	shaderProgram->setUniformValue(textureSamplerUniform, 0);

	// VIDEO

	shaderProgram->setUniformValue(vertexMatrixUniform, videoVertexMatrix);
	shaderProgram->setUniformValue(textureMatrixUniform, videoTextureMatrix);

	videoPanelBuffer->bind();
	videoPanelTexture->bind();

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(vertexCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(textureCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, (void*)(sizeof(GLfloat) * 8));
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	videoPanelBuffer->release();
	videoPanelTexture->release();

	// MAP

	shaderProgram->setUniformValue(vertexMatrixUniform, mapVertexMatrix);
	shaderProgram->setUniformValue(textureMatrixUniform, mapTextureMatrix);

	mapPanelBuffer->bind();
	mapPanelTexture->bind();

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(vertexCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(textureCoordAttribute, 2, GL_FLOAT, GL_FALSE, 0, (void*)(sizeof(GLfloat) * 8));
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	mapPanelBuffer->release();
	mapPanelTexture->release();

	shaderProgram->release();
}

QOpenGLTexture* VideoRenderer::getVideoPanelTexture()
{
	return videoPanelTexture.get();
}