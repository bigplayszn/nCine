#define NCINE_INCLUDE_OPENGL
#include "common_headers.h"
#include "common_macros.h"
#include <nctl/CString.h>
#include "Texture.h"
#include "TextureLoaderRaw.h"
#include "GLTexture.h"
#include "RenderStatistics.h"
#include "tracy.h"

namespace ncine {

GLenum ncFormatToInternal(Texture::Format format)
{
	switch (format)
	{
		case Texture::Format::R8:
			return GL_R8;
		case Texture::Format::RG8:
			return GL_RG8;
		case Texture::Format::RGB8:
			return GL_RGB8;
		case Texture::Format::RGBA8:
		default:
			return GL_RGBA8;
	}
}

GLenum channelsToFormat(int numChannels)
{
	switch (numChannels)
	{
		case 1:
			return GL_RED;
		case 2:
			return GL_RG;
		case 3:
			return GL_RGB;
		case 4:
		default:
			return GL_RGBA;
	}
}

uint32_t *chromaKeyPixels(uint32_t *destBuffer, const unsigned char *srcBuffer, unsigned int numPixels, uint32_t chromaKeyColor)
{
	for (unsigned int i = 0; i < numPixels; i++)
	{
		const unsigned char r = srcBuffer[i * 3 + 0];
		const unsigned char g = srcBuffer[i * 3 + 1];
		const unsigned char b = srcBuffer[i * 3 + 2];
		const uint32_t originalPixel = (b << 16) + (g << 8) + r;
		if (originalPixel == chromaKeyColor)
			destBuffer[i] = 0x00000000;
		else
			destBuffer[i] = 0xFF000000 + originalPixel;
	}
	return destBuffer;
}

///////////////////////////////////////////////////////////
// CONSTRUCTORS and DESTRUCTOR
///////////////////////////////////////////////////////////

Texture::Texture()
    : Object(ObjectType::TEXTURE), glTexture_(nctl::makeUnique<GLTexture>(GL_TEXTURE_2D)),
      width_(0), height_(0), mipMapLevels_(0), isCompressed_(false), numChannels_(0), dataSize_(0),
      minFiltering_(Filtering::NEAREST), magFiltering_(Filtering::NEAREST), wrapMode_(Wrap::CLAMP_TO_EDGE),
      isChromaKeyEnabled_(false), chromaKeyColor_(Color::Magenta)
{
}

Texture::Texture(const char *bufferName, const unsigned char *bufferPtr, unsigned long int bufferSize)
    : Texture()
{
	const bool hasLoaded = loadFromMemory(bufferName, bufferPtr, bufferSize);
	if (hasLoaded == false)
		LOGE_X("Texture \"%s\" cannot be loaded", bufferName);
}

Texture::Texture(const char *filename)
    : Texture()
{
	const bool hasLoaded = loadFromFile(filename);
	if (hasLoaded == false)
		LOGE_X("Texture \"%s\" cannot be loaded", filename);
}

Texture::~Texture()
{
	RenderStatistics::removeTexture(dataSize_);
}

///////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
///////////////////////////////////////////////////////////

void Texture::init(const char *name, Format format, int mipMapCount, int width, int height)
{
	ZoneScoped;
	ZoneText(name, nctl::strnlen(name, nctl::String::MaxCStringLength));

	TextureLoaderRaw texLoader(width, height, mipMapCount, ncFormatToInternal(format));

	if (dataSize_ > 0)
		RenderStatistics::removeTexture(dataSize_);

	glTexture_->bind();
	setName(name);
	setGLTextureLabel(name);
	initialize(texLoader);

	RenderStatistics::addTexture(dataSize_);
}

void Texture::init(const char *name, Format format, int mipMapCount, Vector2i size)
{
	init(name, format, mipMapCount, size.x, size.y);
}

void Texture::init(const char *name, Format format, int width, int height)
{
	init(name, format, 1, width, height);
}

void Texture::init(const char *name, Format format, Vector2i size)
{
	init(name, format, 1, size.x, size.y);
}

bool Texture::loadFromMemory(const char *bufferName, const unsigned char *bufferPtr, unsigned long int bufferSize)
{
	ZoneScoped;
	ZoneText(bufferName, nctl::strnlen(bufferName, nctl::String::MaxCStringLength));

	nctl::UniquePtr<ITextureLoader> texLoader = ITextureLoader::createFromMemory(bufferName, bufferPtr, bufferSize);
	if (texLoader->hasLoaded() == false)
		return false;

	if (dataSize_ > 0)
		RenderStatistics::removeTexture(dataSize_);

	glTexture_->bind();
	setName(bufferName);
	setGLTextureLabel(bufferName);
	initialize(*texLoader);
	load(*texLoader);

	RenderStatistics::addTexture(dataSize_);
	return true;
}

bool Texture::loadFromFile(const char *filename)
{
	ZoneScoped;
	ZoneText(filename, nctl::strnlen(filename, nctl::String::MaxCStringLength));

	nctl::UniquePtr<ITextureLoader> texLoader = ITextureLoader::createFromFile(filename);
	if (texLoader->hasLoaded() == false)
		return false;

	if (dataSize_ > 0)
		RenderStatistics::removeTexture(dataSize_);

	glTexture_->bind();
	setName(filename);
	setGLTextureLabel(filename);
	initialize(*texLoader);
	load(*texLoader);

	RenderStatistics::addTexture(dataSize_);
	return true;
}

bool Texture::loadFromTexels(const unsigned char *bufferPtr)
{
	return loadFromTexels(bufferPtr, 0, 0, 0, width_, height_);
}

bool Texture::loadFromTexels(const unsigned char *bufferPtr, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	return loadFromTexels(bufferPtr, 0, x, y, width, height);
}

bool Texture::loadFromTexels(const unsigned char *bufferPtr, Recti region)
{
	return loadFromTexels(bufferPtr, 0, region.x, region.y, region.w, region.h);
}

bool Texture::loadFromTexels(const unsigned char *bufferPtr, unsigned int level, unsigned int x, unsigned int y, unsigned int width, unsigned int height)
{
	const unsigned char *data = bufferPtr;
	nctl::UniquePtr<uint32_t[]> chromaPixels;

	if (numChannels_ == 3 && isChromaKeyEnabled_)
	{
		numChannels_ = 4;
		const unsigned int numPixels = width * height - (y * width + x);
		chromaPixels = nctl::makeUnique<uint32_t[]>(numPixels);
		chromaKeyPixels(chromaPixels.get(), bufferPtr, numPixels, chromaKeyColor_.abgr());
		data = reinterpret_cast<const unsigned char *>(chromaPixels.get());
	}

	GLenum format = channelsToFormat(numChannels_);
	glGetError();
	glTexture_->texSubImage2D(level, x, y, width, height, format, GL_UNSIGNED_BYTE, data);
	const GLenum error = glGetError();

	return (error == GL_NO_ERROR);
}

bool Texture::loadFromTexels(const unsigned char *bufferPtr, unsigned int level, Recti region)
{
	return loadFromTexels(bufferPtr, level, region.x, region.y, region.w, region.h);
}

void Texture::setMinFiltering(Filtering filter)
{
	GLenum glFilter = GL_NEAREST;
	// clang-format off
	switch (filter)
	{
		case Filtering::NEAREST:				glFilter = GL_NEAREST; break;
		case Filtering::LINEAR:					glFilter = GL_LINEAR; break;
		case Filtering::NEAREST_MIPMAP_NEAREST:	glFilter = GL_NEAREST_MIPMAP_NEAREST; break;
		case Filtering::LINEAR_MIPMAP_NEAREST:	glFilter = GL_LINEAR_MIPMAP_NEAREST; break;
		case Filtering::NEAREST_MIPMAP_LINEAR:	glFilter = GL_NEAREST_MIPMAP_LINEAR; break;
		case Filtering::LINEAR_MIPMAP_LINEAR:	glFilter = GL_LINEAR_MIPMAP_LINEAR; break;
		default:								glFilter = GL_NEAREST; break;
	}
	// clang-format on

	glTexture_->bind();
	glTexture_->texParameteri(GL_TEXTURE_MIN_FILTER, glFilter);
	minFiltering_ = filter;
}

void Texture::setMagFiltering(Filtering filter)
{
	GLenum glFilter = GL_NEAREST;
	// clang-format off
	switch (filter)
	{
		case Filtering::NEAREST:				glFilter = GL_NEAREST; break;
		case Filtering::LINEAR:					glFilter = GL_LINEAR; break;
		default:								glFilter = GL_NEAREST; break;
	}
	// clang-format on

	glTexture_->bind();
	glTexture_->texParameteri(GL_TEXTURE_MAG_FILTER, glFilter);
	magFiltering_ = filter;
}

void Texture::setWrap(Wrap wrapMode)
{
	GLenum glWrap = GL_CLAMP_TO_EDGE;
	// clang-format off
	switch (wrapMode)
	{
		case Wrap::CLAMP_TO_EDGE:				glWrap = GL_CLAMP_TO_EDGE; break;
		case Wrap::MIRRORED_REPEAT:				glWrap = GL_MIRRORED_REPEAT; break;
		case Wrap::REPEAT:						glWrap = GL_REPEAT; break;
		default:								glWrap = GL_CLAMP_TO_EDGE; break;
	}
	// clang-format on

	glTexture_->bind();
	glTexture_->texParameteri(GL_TEXTURE_WRAP_S, glWrap);
	glTexture_->texParameteri(GL_TEXTURE_WRAP_T, glWrap);
	wrapMode_ = wrapMode;
}

void *Texture::imguiTexId()
{
	return reinterpret_cast<void *>(glTexture_.get());
}

///////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
///////////////////////////////////////////////////////////

void Texture::initialize(const ITextureLoader &texLoader)
{
	const IGfxCapabilities &gfxCaps = theServiceLocator().gfxCapabilities();
	const int maxTextureSize = gfxCaps.value(IGfxCapabilities::GLIntValues::MAX_TEXTURE_SIZE);
	FATAL_ASSERT_MSG_X(texLoader.width() <= maxTextureSize, "Texture width %d is bigger than device maximum %d", texLoader.width(), maxTextureSize);
	FATAL_ASSERT_MSG_X(texLoader.height() <= maxTextureSize, "Texture height %d is bigger than device maximum %d", texLoader.height(), maxTextureSize);

	glTexture_->texParameteri(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexture_->texParameteri(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	wrapMode_ = Wrap::CLAMP_TO_EDGE;

	if (texLoader.mipMapCount() > 1)
	{
		glTexture_->texParameteri(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexture_->texParameteri(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		magFiltering_ = Filtering::LINEAR;
		minFiltering_ = Filtering::LINEAR_MIPMAP_LINEAR;
		// To prevent artifacts if the MIP map chain is not complete
		glTexture_->texParameteri(GL_TEXTURE_MAX_LEVEL, texLoader.mipMapCount());
	}
	else
	{
		glTexture_->texParameteri(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexture_->texParameteri(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		magFiltering_ = Filtering::LINEAR;
		minFiltering_ = Filtering::LINEAR;
	}

	const TextureFormat &texFormat = texLoader.texFormat();
	unsigned int numChannels = texLoader.bpp();
	GLenum internalFormat = texFormat.internalFormat();
	GLenum format = texFormat.format();
	unsigned long dataSize = texLoader.dataSize();
	if (texFormat.isCompressed() == false && numChannels == 3 && isChromaKeyEnabled_)
	{
		numChannels = 4;
		internalFormat = GL_RGBA8;
		format = GL_RGBA;
		dataSize = texLoader.width() * texLoader.height() * 4;
	}

#if (defined(__ANDROID__) && GL_ES_VERSION_3_0) || defined(WITH_ANGLE) || defined(__EMSCRIPTEN__)
	const bool withTexStorage = true;
#else
	const bool withTexStorage = gfxCaps.hasExtension(IGfxCapabilities::GLExtensions::ARB_TEXTURE_STORAGE);
#endif

	if (withTexStorage && dataSize_ > 0 &&
	    (width_ != texLoader.width() || height_ != texLoader.height() || numChannels_ != numChannels))
	{
		// The OpenGL texture needs to be recreated as its storage is immutable
		glTexture_ = nctl::makeUnique<GLTexture>(GL_TEXTURE_2D);
	}

	if (withTexStorage)
		glTexture_->texStorage2D(texLoader.mipMapCount(), internalFormat, texLoader.width(), texLoader.height());
	else if (texFormat.isCompressed() == false)
	{
		int levelWidth = texLoader.width();
		int levelHeight = texLoader.height();

		for (int i = 0; i < texLoader.mipMapCount(); i++)
		{
			glTexture_->texImage2D(i, internalFormat, levelWidth, levelHeight, format, texFormat.type(), nullptr);
			levelWidth /= 2;
			levelHeight /= 2;
		}
	}

	width_ = texLoader.width();
	height_ = texLoader.height();
	mipMapLevels_ = texLoader.mipMapCount();
	isCompressed_ = texFormat.isCompressed();
	numChannels_ = numChannels;
	dataSize_ = dataSize;
}

void Texture::load(const ITextureLoader &texLoader)
{
#if (defined(__ANDROID__) && GL_ES_VERSION_3_0) || defined(WITH_ANGLE) || defined(__EMSCRIPTEN__)
	const bool withTexStorage = true;
#else
	const IGfxCapabilities &gfxCaps = theServiceLocator().gfxCapabilities();
	const bool withTexStorage = gfxCaps.hasExtension(IGfxCapabilities::GLExtensions::ARB_TEXTURE_STORAGE);
#endif

	const TextureFormat &texFormat = texLoader.texFormat();
	int levelWidth = width_;
	int levelHeight = height_;

	GLenum format = texFormat.format();
	const unsigned char *data = texLoader.pixels();
	nctl::UniquePtr<uint32_t[]> chromaPixels;

	for (int mipIdx = 0; mipIdx < texLoader.mipMapCount(); mipIdx++)
	{
		if (texFormat.isCompressed() == false && texFormat.numChannels() == 3 && isChromaKeyEnabled_)
		{
			format = GL_RGBA;
			const unsigned int numPixels = levelWidth * levelHeight;
			chromaPixels = nctl::makeUnique<uint32_t[]>(numPixels);
			chromaKeyPixels(chromaPixels.get(), texLoader.pixels(mipIdx), numPixels, chromaKeyColor_.abgr());
			data = reinterpret_cast<const unsigned char *>(chromaPixels.get());
		}

		if (texFormat.isCompressed())
		{
			if (withTexStorage)
				glTexture_->compressedTexSubImage2D(mipIdx, 0, 0, levelWidth, levelHeight, texFormat.internalFormat(), texLoader.dataSize(mipIdx), texLoader.pixels(mipIdx));
			else
				glTexture_->compressedTexImage2D(mipIdx, texFormat.internalFormat(), levelWidth, levelHeight, texLoader.dataSize(mipIdx), texLoader.pixels(mipIdx));
		}
		else
			// Storage has already been created at this point
			glTexture_->texSubImage2D(mipIdx, 0, 0, levelWidth, levelHeight, format, texFormat.type(), data);

		levelWidth /= 2;
		levelHeight /= 2;
	}
}

void Texture::setGLTextureLabel(const char *filename)
{
	glTexture_->setObjectLabel(filename);
}

}
