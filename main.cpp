#include <iostream>
#include <fstream>
#include <math.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <glm/glm.hpp>

enum ImageType : uint8_t {
    BLACKWHITE, GRAYSCALE, COLOR, QUANTIZED
};

namespace Convolution {
    template<typename InT, typename OutT>
    OutT* PadBuffer(InT* buffer, int32_t width, int32_t height, int32_t cropX, int32_t cropY, OutT value)
    {
        int32_t outWidth = width + 2 * cropX;
        int32_t outHeight = height + 2 * cropY;
        OutT* outBuffer = (OutT*) malloc(outWidth * outHeight * sizeof(OutT));
        for(int32_t y = 0; y < outHeight; y++)
        {
            for(int32_t x = 0; x < outWidth; x++)
            {
                int32_t imageX = x - cropX;
                int32_t imageY = y - cropY;
                size_t outIndex = y * outWidth + x;
                if(imageX >= 0 && imageX < width && imageY >= 0 && imageY < height) {
                    size_t imageIndex = imageY * width + imageX;
                    outBuffer[outIndex] = buffer[imageIndex];
                }else{
                    outBuffer[outIndex] = value;
                }
            }
        }
        return outBuffer;
    }

    template<typename T>
    T* Crop(T* buffer, int32_t outWidth, int32_t outHeight, int32_t cropX, int32_t cropY)
    {
        T* outBuffer = (T*) malloc(outWidth * outHeight * sizeof(T));
        int32_t originalWidth = outWidth + 2 * cropX;
        for(int32_t y = 0; y < outHeight; y++)
        {
            for(int32_t x = 0; x < outWidth; x++)
            {
                outBuffer[y * outWidth + x] = buffer[(y + cropY) * originalWidth + (x + cropX)];
            }
        }
        return outBuffer;
    }

    template<typename OutT, typename InT1, typename InT2>
    OutT* Convolve(InT1* buffer, int32_t width, int32_t height, InT2* filter, int32_t fWidth, int32_t fHeight, bool flip, bool cropToOriginal, std::function<OutT(float)> pixelChangeFn)
    {
        float halfX = (fWidth - 1) / 2;
        float halfY = (fHeight - 1) / 2;
        int32_t outWidth = width + 2 * halfX;
        int32_t outHeight = height + 2 * halfY;
        InT1* paddedBuffer = PadBuffer<InT1, InT1>(buffer, width, height, halfX, halfY, 0);
        OutT* outputBuffer = (OutT*) calloc(outWidth * outHeight, sizeof(OutT));
        for(int32_t y = 0; y < outHeight; y++)
        {
            for(int32_t x = 0; x < outWidth; x++) {
                size_t outIndex = y * outWidth + x;
                float pixelValue = 0;
                for(int fy = 0; fy < fHeight; fy++)
                {
                    int ny = y + fy - halfY;
                    if(ny < 0 || ny >= outHeight) continue;

                    for(int fx = 0; fx < fWidth; fx++)
                    {
                        int nx = x + fx - halfX;
                        if(nx < 0 || nx >= outWidth) continue;
                        
                        size_t filterIndex = flip ? (fHeight - 1 - fy) * fWidth + (fWidth - 1 - fx) : fy * fWidth + fx;

                        float filterValue = filter[filterIndex];
                        pixelValue += filterValue * paddedBuffer[ny * outWidth + nx];
                    }
                }

                outputBuffer[outIndex] = pixelChangeFn(pixelValue);
            }
        }
        free(paddedBuffer);
        if(cropToOriginal) {
            OutT* temp = Crop<OutT>(outputBuffer, width, height, halfX, halfY);
            free(outputBuffer);
            return temp;
        }
        return outputBuffer;
    }
}

class Image
{
public:
    Image(const std::string& filename, int32_t reqComps)
    {
        m_Data = stbi_load(filename.c_str(), &m_Width, &m_Height, &m_Comps, reqComps);
    }

    Image(uint8_t* data, int32_t width, int32_t height, int32_t comps, ImageType type)
        : m_Type(type), m_Data(data), m_Width(width), m_Height(height), m_Comps(comps) {}

    ~Image() {
        free(m_Data);
    }

    void Save(const std::string& filename)
    {
        std::string pngFilename = "./imagesOut/" + filename + ".png";
        int32_t result = stbi_write_png(pngFilename.c_str(), m_Width, m_Height, m_Comps, m_Data, m_Width * m_Comps);
        if(!result) {
            perror("stbi_write_png failed");
        }
        std::ofstream outputFile("./assignment/" + filename + ".txt");
        for(int32_t y = 0; y < m_Height; y++) {
            for(int32_t x = 0; x < m_Width; x++) {
                if(m_Type == ImageType::GRAYSCALE || m_Type == ImageType::COLOR) {
                    outputFile << static_cast<int32_t>(m_Data[y * m_Width + x]) << ",";
                }else if(m_Type == ImageType::BLACKWHITE) {
                    outputFile << (m_Data[y * m_Width + x] == 255 ? "1" : "0") << ",";
                }else if(m_Type == ImageType::QUANTIZED) {
                    outputFile << static_cast<int32_t>(static_cast<float>(m_Data[y * m_Width + x]) / 16) << ",";
                }
            }
        }
        outputFile.flush();
        outputFile.close();
    }
    
    static unsigned char Clip(float pixel)
    {
        return (unsigned char)glm::clamp((int)pixel, 0, 255);
    }

    Image Convolve(float* data, int32_t fWidth, int32_t fHeight) const {
        uint8_t* convolvedImage = Convolution::Convolve<uint8_t>(m_Data, m_Width, m_Height, data, fWidth, fHeight, true, true, Clip);
        return Image(convolvedImage, m_Width, m_Height, 1, ImageType::GRAYSCALE);
    }

    Image ToGrayscale() const {
        uint8_t* pixelBuffer = (uint8_t*) malloc(m_Width * m_Height * sizeof(int8_t));
        for(int32_t y = 0; y < m_Height; y++)
        {
            for(int32_t x = 0; x < m_Width; x++)
            {
                size_t index = m_Width * y + x;
                uint8_t red = m_Data[index * m_Comps + 0];
                uint8_t green = m_Data[index * m_Comps + 1];
                uint8_t blue = m_Data[index * m_Comps + 2];
                int32_t gray = static_cast<int32_t>(red * 0.2989 + green * 0.5870 + blue * 0.1140);
                pixelBuffer[index] = glm::clamp(gray, 0, 255);
            }
        }
        return Image(pixelBuffer, m_Width, m_Height, 1, ImageType::GRAYSCALE);
    }

    Image ToHalftone() const {
        uint8_t* pixelBuffer = (uint8_t*) malloc(m_Width * m_Height * 4 * sizeof(int8_t));
        int32_t outWidth = 2 * m_Width;
        Image grayscaleIm = ToGrayscale();
        uint8_t* grayscale = grayscaleIm.GetData();
        for(int32_t y = 0; y < m_Height; y++)
        {
            for(int32_t x = 0; x < m_Width; x++)
            {
                size_t index = m_Width * y + x;
                int32_t gray = grayscale[index];
                int32_t pixelValue = glm::clamp(gray, 0, 255);
                size_t outIndex = 2 * y * outWidth + 2 * x;
                pixelBuffer[outIndex + 0] =  pixelValue >= 204 ? 255 : 0;
                pixelBuffer[outIndex + 1] =  pixelValue >= 102 ? 255 : 0;
                pixelBuffer[outIndex + outWidth + 0] = pixelValue >= 51 ? 255 : 0;
                pixelBuffer[outIndex + outWidth + 1] = pixelValue >= 153 ? 255 : 0;
            }
        }
        return Image(pixelBuffer, m_Width * 2, m_Height * 2, 1, ImageType::BLACKWHITE);
    }

    Image ToFloyedSteinberg() const {
        Image grayscaleImage = ToGrayscale();
        float* grayscale = (float*) malloc(m_Width * m_Height * sizeof(float));
        uint8_t* pixelBuffer = (uint8_t*) malloc(m_Width * m_Height * sizeof(int8_t));
        for(int32_t y = 0; y < m_Height; y++)
        {
            for(int32_t x = 0; x < m_Width; x++)
            {
                size_t index = y * m_Width + x;
                grayscale[index] = static_cast<float>(grayscaleImage.m_Data[index]);
            }
        }
        for(int32_t y = 0; y < m_Height; y++)
        {
            for(int32_t x = 0; x < m_Width; x++)
            {
                size_t index = y * m_Width + x;
                float originalValue = grayscale[index];

                float quantizedValue = glm::clamp(static_cast<int32_t>(static_cast<float>(originalValue) / 16) * 17, 0, 255);
                float error = originalValue - quantizedValue;
                float bonus = 0;
                if(x == 0) {
                    bonus = 1.0f/16;
                }else if(x == m_Width - 1) {
                    bonus = 4.0f/16;
                } else if(y == m_Height - 1) {
                    bonus = 9.0f/16;
                }
                if(x < m_Width - 1)
                    grayscale[index + 1] += error * (7.0f/16 + bonus);
                if(x > 0 && y < m_Height - 1)
                    grayscale[index + m_Width - 1] += error * (3.0f/16 + bonus);
                if(y < m_Height - 1)
                    grayscale[index + m_Width] += error * (5.0f/16 + bonus);
                if(x < m_Width - 1 && y < m_Height - 1)
                    grayscale[index + m_Width + 1] += error * (1.0f/16 + bonus);
                pixelBuffer[index] = quantizedValue;
            }
        }
        free(grayscale);
        return Image(pixelBuffer, m_Width, m_Height, 1, ImageType::QUANTIZED);
    }

    Image ToCanny() const {
        float guassian[] =  { 1.0/16,2.0/16,1.0/16,
                                    2.0/16,4.0/16,2.0/16, 
                                    1.0/16,2.0/16,1.0/16};
        float gradientX[] = {1, 0, -1,  
                            2, 0, -2, 
                            1, 0, -1};
        float gradientY[] = { 1, 2, 1, 
                            0, 0, 0, 
                            -1,-2,-1};

        Image grayscale = ToGrayscale();
        uint8_t* grayscaleData = grayscale.GetData();
        int32_t width = GetWidth();
        int32_t height = GetHeight();
        float* noiseAndGradientX = Convolution::Convolve<float>(guassian, 3, 3, gradientX, 3, 3, false, false, [](float x) { return x; });
        float* noiseAndGradientY = Convolution::Convolve<float>(guassian, 3, 3, gradientY, 3, 3, false, false, [](float x) { return x; });
        float* imGradientX = Convolution::Convolve<float>(grayscaleData, width, height, noiseAndGradientX, 5, 5, false, true, [](float x) { return x; });
        float* imGradientY = Convolution::Convolve<float>(grayscaleData, width, height, noiseAndGradientY, 5, 5, false, true, [](float x) { return x; });
        grayscale.Convolve(noiseAndGradientX, 5, 5).Save("noise-and-gradientX");
        grayscale.Convolve(noiseAndGradientY, 5, 5).Save("noise-and-gradientY");

        float* norm = (float*) calloc(width * height, sizeof(float));
        float* angle = (float*) calloc(width * height, sizeof(float));
        float* nms = (float*) calloc(width * height, sizeof(float));
        float* dt = (float*) calloc(width * height, sizeof(float));
        uint8_t* normIm = (uint8_t*) calloc(width * height, sizeof(uint8_t));
        uint8_t* nmsIm = (uint8_t*) calloc(width * height, sizeof(uint8_t));
        uint8_t* dtIm = (uint8_t*) calloc(width * height, sizeof(uint8_t));
        uint8_t* hys = (uint8_t*) calloc(width * height, sizeof(uint8_t));

        float lowRange = 150;
        float highRange = 200;

        // Norm

        for(int32_t y = 0; y < height; y++)
        {
            for(int32_t x = 0; x < width; x++)
            {
                float gradX = imGradientX[y * width + x];
                float gradY = imGradientY[y * width + x];
                norm[y * width + x] = glm::sqrt(gradX * gradX + gradY * gradY);
                angle[y * width + x] = atan2(gradY, gradX);
                normIm[y * width + x] = Clip(norm[y * width + x]);
            }
        }
        Image(normIm, width, height, 1, ImageType::GRAYSCALE).Save("norm");

        // NMS + Double thresholding

        int32_t xOffset1[] = {-1, -1,  0,  1};
        int32_t yOffset1[] = { 0, -1, -1, -1};
        int32_t xOffset2[] = { 1,  1,  0, -1};
        int32_t yOffset2[] = { 0,  1,  1,  1};

        for(int32_t y = 1; y < height - 1; y++)
        {
            for(int32_t x = 1; x < width - 1; x++)
            {
                float currentAngle = angle[y * width + x];
                int index = 0;

                if(!(-2.748 <= currentAngle && currentAngle <= 2.748) || (-0.392 <= currentAngle && currentAngle <= 0.392)) {
                    index = 0;
                }else if((0.392 <= currentAngle && currentAngle <= 1.178) || (-2.748 <= currentAngle && currentAngle <= -1.964)) {
                    index = 1;
                }else if((1.178 <= currentAngle && currentAngle <= 1.963) || (-1.963 <= currentAngle && currentAngle <= -1.178)) {
                    index = 2;
                }else if((1.963 <= currentAngle && currentAngle <= 2.748) || (-1.178 <= currentAngle && currentAngle <= -0.392)) {
                    index = 3;
                }
                float n1 = 0;
                if(y + yOffset1[index] >= 0 && y + yOffset1[index] < height && x + xOffset1[index] >= 0 && x + xOffset1[index] < width) {
                    n1 = norm[(y + yOffset1[index]) * width + (x + xOffset1[index])];
                }
                float n2 = 0;
                if(y + yOffset2[index] >= 0 && y + yOffset2[index] < height && x + xOffset2[index] >= 0 && x + xOffset2[index] < width) {
                    n2 = norm[(y + yOffset2[index]) * width + (x + xOffset2[index])];
                }

                float current = norm[y * width + x];
                if(n1 > current || n2 > current) {
                    nms[y * width + x] = 0;
                    nmsIm[y * width + x] = 0;
                }else{
                    nms[y * width + x] = current;
                    nmsIm[y * width + x] = Clip(current);
                }
                
                if(nmsIm[y * width + x] >= lowRange) {
                    dt[y * width + x] = current;
                    dtIm[y * width + x] = Clip(current);
                }else{
                    dt[y * width + x] = 0;
                    dtIm[y * width + x] = 0;
                }
                
            }
        }
        Image(nmsIm, width, height, 1, ImageType::GRAYSCALE).Save("nms");
        
        // Hysteresis

        memcpy(hys, dtIm, width * height * sizeof(uint8_t));
        Image(dtIm, width, height, 1, ImageType::GRAYSCALE).Save("dt");
        int32_t offsetsX[] = {-1, 0, 1, -1, 0, 1, -1, 0, 1};
        int32_t offsetsY[] = {-1, -1, -1, 0, 0, 0, 1, 1, 1};
        for(int j = 0; j < 2; j++) {
            for(int32_t y = 0; y < height; y++)
            {
                for(int32_t x = 0; x < width; x++)
                {
                    size_t index = y * width + x;

                    if(hys[index] < lowRange) continue;

                    bool hasStrongEdge = false; 
                    for(int32_t i = 0; i < 9; i++) {
                        int32_t currOffX = offsetsX[i];
                        int32_t currOffY = offsetsY[i];
                        
                        if(y + currOffY >= 0 && y + currOffY < height && x + currOffX >= 0 && x + currOffX < width) {
                            size_t nOffset = (y + currOffY) * width + (x + currOffX);
                            if(hys[nOffset] >= highRange) {
                                hasStrongEdge = true;
                                break;
                            }
                        }
                    }
                    if(hasStrongEdge) {
                        hys[index] = 255;
                    }
                    else if (j == 1 && hys[index] <= highRange)
                    {
                        hys[index] = 0;
                    }
                }
            }
        }

        free(norm);
        free(angle);
        free(nms);
        free(dt);

        free(noiseAndGradientX);
        free(noiseAndGradientY);
        free(imGradientX);
        free(imGradientY);

        return Image(hys, width, height, 1, ImageType::BLACKWHITE);
    }

    ImageType GetType() const { return m_Type; }
    uint8_t* GetData() const { return m_Data; }
    int32_t GetWidth() const { return m_Width; }
    int32_t GetHeight() const { return m_Height; }
private:
    ImageType m_Type;
    uint8_t* m_Data;
    int32_t m_Width, m_Height, m_Comps;
};

int main()
{
    Image im("./Lenna.png", 4);
    im.ToGrayscale().Save("Grayscale");
    im.ToCanny().Save("Canny");
    im.ToHalftone().Save("Halftone");
    im.ToFloyedSteinberg().Save("FloydSteinberg");    
}