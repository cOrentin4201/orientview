// Copyright � 2014 Mikko Ronkainen <firstname@mikkoronkainen.com>
// License: GPLv3, see the LICENSE file.

#include <QStyle>
#include <QApplication>
#include <QDesktopWidget>

#include "VideoWindow.h"
#include "VideoDecoder.h"

using namespace OrientView;

VideoWindow::VideoWindow(QWindow* parent) : QWindow(parent)
{
}

bool VideoWindow::initialize(VideoDecoder* videoDecoder)
{
	qDebug("Initializing VideoWindow");

	setSurfaceType(QWindow::OpenGLSurface);
	setIcon(QIcon(":/MainView/misc/orientview.ico"));
	setTitle("OrientView - Video");
	resize(videoDecoder->getFrameWidth(), videoDecoder->getFrameHeight());
	setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), QApplication::desktop()->availableGeometry()));

	qDebug("Creating OpenGL context");

	context = std::unique_ptr<QOpenGLContext>(new QOpenGLContext());
	context->setFormat(format());

	if (!context->create())
	{
		qWarning("Could not create OpenGL context");
		return false;
	}

	if (!context->makeCurrent(this))
	{
		qWarning("Could not make context current");
		return false;
	}

	return true;
}

void VideoWindow::shutdown()
{
	qDebug("Shutting down VideoWindow");

	if (context != nullptr)
		context.reset(nullptr);
}

QOpenGLContext* VideoWindow::getContext() const
{
	return context.get();
}

bool VideoWindow::event(QEvent* event)
{
	if (event->type() == QEvent::Close)
		emit closing();

	return QWindow::event(event);
}