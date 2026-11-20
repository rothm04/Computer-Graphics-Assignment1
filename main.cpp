#include <iostream>
#include <functional>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <glm/glm.hpp>

#include <fstream>
#include <string>

template<typename InT, typename OutT>
OutT* PerformConvolution(InT* data, int width, int height, float* filter, int size, std::function<OutT(float)> funcPixelChange);

class Image {
public:
    Image(const char* filename, int reqComps)
    {
        m_Data = stbi_load(filename, &m_Width, &m_Height, &m_Comps, reqComps);
    }

    Image(unsigned char* ogData, int width, int height, int comps)
    : m_Data(ogData), m_Width(width), m_Height(height), m_Comps(comps) {}

    void Save(std::string filename)
    {   
        std::string filenamePng = "./" + filename + ".png";
        stbi_write_png(filenamePng.c_str(), m_Width, m_Height, m_Comps, m_Data, m_Width * m_Comps);
        std::ofstream debugFile("./assignment/" + filename + ".txt");
        for(int y = 0; y < m_Height; y++) {
            for(int x = 0; x < m_Width; x++) {
                debugFile << (int)(m_Data[x + y *m_Width]) << ",";
            }
        }
        debugFile.close();
    }
    
    static unsigned char Clip(float pixel)
    {
        return (unsigned char)glm::clamp((int)pixel, 0, 255);
    }

    void ClipImg()
    {
        for(int y = 0; y < m_Height; y++)
        {
            for(int x = 0; x < m_Width; x++)
            {
                m_Data[y * m_Width + x] = Clip(m_Data[y * m_Width + x]);
            }
        }
    }

    // ----------
    // |  |-width  |  |
    // ----------
    // |-1|x |+1|
    // ----------
    // |  |width|  |
    // ----------
    Image Convolve(float* filter, int size) const {
        unsigned char* data = PerformConvolution<unsigned char, unsigned char>(m_Data, m_Width, m_Height, filter, size, Clip);
        return Image(data, m_Width, m_Height, 1);
    }

    const unsigned char* GetData() const 
    {
        return m_Data; 
    }

    const int GetWidth() const { return m_Width; }
    const int GetHeight() const { return m_Height; }
    const int GetComps() const { return m_Comps; }
private:
    unsigned char* m_Data;
    int m_Width, m_Height, m_Comps;
};

Image MakeGrayscale(const Image& ogPic);
Image MakeCannyEdge(const Image& ogPic);

int main(int argc, char** argv)
{
    Image im("./Lenna.png", 4);
    Image grayscale = MakeGrayscale(im);
    Image canny = MakeCannyEdge(grayscale);
    grayscale.Save("output_grayscale");
    canny.Save("output_canny12");
    std::cout << "Hello world" << std::endl; 
}

Image MakeGrayscale(const Image& ogPic)
{   
    int width = ogPic.GetWidth();
    int height = ogPic.GetHeight();
    int comps = ogPic.GetComps();

    unsigned char* grayscaleData = (unsigned char*) malloc(sizeof(unsigned char) * width * height);
    const unsigned char* ogData = ogPic.GetData();
    size_t index = 0;
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            index = y * width + x;
            unsigned char red = ogData[index * comps  + 0];
            unsigned char green = ogData[index * comps + 1];
            unsigned char blue = ogData[index * comps + 2];
            // unsigned char max = glm::max(glm::max(red, green), blue);
            // unsigned char min = glm::min(glm::min(red, green), blue);
            // grayscaleData[index] = (max + min) / 2;
            // grayscaleData[index] = static_cast<float>(red + green + blue) / 3;
            // grayscaleData[index] = (red * 0.2126 + green * 0.7152 + blue * 0.0722);
            grayscaleData[index] = (red * 0.2989 + green * 0.5870 + blue * 0.1140);

        }
    }
    return Image(grayscaleData, width, height, 1);
} 

template<typename InT, typename OutT>
OutT* PerformConvolution(InT* data, int width, int height, float* filter, int size, std::function<OutT(float)> funcPixelChange)
{
    OutT* newData = (OutT*)malloc(width * height * sizeof(OutT));
    for(int y = 0; y < height; y++)
    {
        for(int x = 0; x < width; x++)
        {
            int index = y * width + x;
            float pixelValue = 0;
            int half = size / 2;
            for(int fy = 0; fy < size; fy++)
            {
                int ny = y + fy - half;
                if (ny < 0 || ny >= height) continue;

                for(int fx = 0; fx < size; fx++)
                {
                    int nx = x + fx - half;
                    if (nx < 0 || nx >= width) continue;
                    float filterValue = filter[fy * size + fx];
                    //float filterValue = filter[(size - 1 - fy) * size + (size - 1 - fx)];
                    pixelValue += filterValue * data[ny * width + nx];
                }
            }
            newData[index] = funcPixelChange(pixelValue);
        }
    }
    return newData;
}

// float* PadNum(float* filter, int& height, int& width,  float num)
// {
//     float* newFilter = (float*)malloc((width + 2)*(height + 2)* sizeof(float));
//     for(int i = 0; i < width + 2; i++)
//     {
//         newFilter[0 + i] = num;
//         newFilter[(height + 1) *(width+2) + i] = num;
//         newFilter[(width + 1) + i*(width * 2)] = num;
//         newFilter[0 + i*(width * 2)] = num;
//     }
//     for (int y = 1; y < height + 1; y++)
//     {
//         for(int x = 1; x < width + 1; x++)
//         {
//             newFilter[y * (width + 2) + x] = filter[(y-1) * width + (x-1)];
//         }
//     }
//     width += 2;
//     height += 2;
//     return newFilter;
// }

template<typename T>
T* PadZeros(const T* data, int width, int height)
{
    int pw = width + 2;
    int ph = height + 2;
    T* padded = (T*)calloc(pw * ph, sizeof(T)); // zero-initializes all 4 borders

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            padded[(y + 1) * pw + (x + 1)] = data[y * width + x];
        }
    }
    return padded;
}

template<typename T>
T* CropBorder(const T* padded, int width, int height)
{
    int pw = width + 2;
    T* cropped = (T*)malloc(width * height * sizeof(T));

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            cropped[y * width + x] = padded[(y + 1) * pw + (x + 1)];
        }
    }
    return cropped;
}

float* FlipKernel(const float* filter, int size)
{
    int total = size * size;
    float* flipped = (float*)malloc(total * sizeof(float));
    for (int i = 0; i < total; i++)
    {
        flipped[i] = filter[total - 1 - i];
    }
    return flipped;
}

// -1 0 1         1 0 -1
// -2 0 2    ->   2 0 -2
// -1 0 1         1 0 -1
//


// Original ->grayscale -> Blur(gaussian noise reduction) ->  gradient intensity (gradien calculations)
    // -> NMS(non-max to sharpen the edge) -> double thresholding (weak and strong edge detection) 
    // -> hysteresis (edge tracking and connectiong loose edges) 
Image MakeCannyEdge(const Image& ogPic)
{
    // (G_x * B) * I 
    float noiseReduction[] =  { 1.0/16,2.0/16,1.0/16,
                                2.0/16,4.0/16,2.0/16, 
                                1.0/16,2.0/16,1.0/16};
    float gradientX[] = {1, 0, -1,  
                         2, 0, -2, 
                         1, 0, -1};
    float gradientY[] = { 1, 2, 1, 
                          0, 0, 0, 
                         -1,-2,-1};

   // Cross-correlation version (if your Convolve() does NOT flip kernels)
    float noiseAndGradientX2[25] = {
    1.0f/16,  2.0f/16, 0,  -2.0f/16, -1.0f/16,
    4.0f/16,  8.0f/16, 0,  -8.0f/16, -4.0f/16,
    6.0f/16, 12.0f/16, 0, -12.0f/16, -6.0f/16,
    4.0f/16,  8.0f/16, 0,  -8.0f/16, -4.0f/16,
    1.0f/16,  2.0f/16, 0,  -2.0f/16, -1.0f/16
    };

    // Gradient * Guass = 5x5
    
  
   // float* noiseAndGradientX = PerformConvolution<float, float>(noiseReduction, 3, 3, gradientX, 3, [](float x){return x;});
    // float* noiseAndGradientX = PerformConvolution<float, float>(PadZeros<float>(noiseReduction, 3, 3), 5, 5, FlipKernel(gradientX, 3), 3, [](float x){return x;}); //todo: make sure all the frees are there
    // for(int i =0 ; i < 25; i++)
    // {
    //     if(noiseAndGradientX2[i] != noiseAndGradientX[i])
    //     {
    //         std::cout << "hey stupid, " << i << " is different " << noiseAndGradientX2[i]  << " != " << noiseAndGradientX[i] << std::endl;
    //     }
    // }

    // Image result =  ogPic.Convolve(noiseAndGradientX, 5);
    
    //
    // template<typename InT, typename OutT>
    // OutT* PerformConvolution(InT* data, int width, int height, float* filter, int size, std::function<OutT(float)> funcPixelChange)
     int width = ogPic.GetWidth();
     int height = ogPic.GetHeight();
    // float* noise = PerformConvolution<const unsigned char, float>(ogPic.GetData(), width, height, noiseReduction, 3, [](float x) {return x;});
    // float* newNoise = PadZeros(noise, height, width); //ToDo: make sure not to forget about malloc deletions 
    // unsigned char* afterGradientX = PerformConvolution<float, unsigned char>(newNoise, width, height, gradientX, 3, Image::Clip);
    // return Image(afterGradientX, ogPic.GetWidth(), ogPic.GetHeight(), 1);

    unsigned char* padded = PadZeros<unsigned char>(ogPic.GetData(), width, height);
    float* noise = PerformConvolution<unsigned char, float>(padded, width + 2, height + 2, noiseReduction, 3, [](float x) { return x; });
    unsigned char* gradPadded = PerformConvolution<float, unsigned char>(noise, width + 2, height + 2, gradientX, 3, Image::Clip);
    unsigned char* afterGradientX = CropBorder<unsigned char>(gradPadded, width, height);

    free(noise);
    free(padded);
    free(gradPadded);
    Image result =  Image(afterGradientX, width, height, 1);
    return result;
}