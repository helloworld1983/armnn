//
// Copyright © 2017 Arm Ltd. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include "ImageTensorGenerator.hpp"
#include "../InferenceTestImage.hpp"
#include <armnn/Logging.hpp>
#include <armnn/TypesUtils.hpp>

#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/program_options.hpp>
#include <boost/variant.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

// parses the command line to extract
// * the input image file -i the input image file path (must exist)
// * the layout -l the data layout output generated with (optional - default value is NHWC)
// * the output file -o the output raw tensor file path (must not already exist)
class CommandLineProcessor
{
public:
    bool ValidateInputFile(const std::string& inputFileName)
    {
        if (inputFileName.empty())
        {
            std::cerr << "No input file name specified" << std::endl;
            return false;
        }

        if (!boost::filesystem::exists(inputFileName))
        {
            std::cerr << "Input file [" << inputFileName << "] does not exist" << std::endl;
            return false;
        }

        if (boost::filesystem::is_directory(inputFileName))
        {
            std::cerr << "Input file [" << inputFileName << "] is a directory" << std::endl;
            return false;
        }

        return true;
    }

    bool ValidateLayout(const std::string& layout)
    {
        if (layout.empty())
        {
            std::cerr << "No layout specified" << std::endl;
            return false;
        }

        std::vector<std::string> supportedLayouts = { "NHWC", "NCHW" };

        auto iterator = std::find(supportedLayouts.begin(), supportedLayouts.end(), layout);
        if (iterator == supportedLayouts.end())
        {
            std::cerr << "Layout [" << layout << "] is not supported" << std::endl;
            return false;
        }

        return true;
    }

    bool ValidateOutputFile(std::string& outputFileName)
    {
        if (outputFileName.empty())
        {
            std::cerr << "No output file name specified" << std::endl;
            return false;
        }

        if (boost::filesystem::exists(outputFileName))
        {
            std::cerr << "Output file [" << outputFileName << "] already exists" << std::endl;
            return false;
        }

        if (boost::filesystem::is_directory(outputFileName))
        {
            std::cerr << "Output file [" << outputFileName << "] is a directory" << std::endl;
            return false;
        }

        boost::filesystem::path outputPath(outputFileName);
        if (!boost::filesystem::exists(outputPath.parent_path()))
        {
            std::cerr << "Output directory [" << outputPath.parent_path().c_str() << "] does not exist" << std::endl;
            return false;
        }

        return true;
    }

    bool ProcessCommandLine(int argc, char* argv[])
    {
        namespace po = boost::program_options;

        po::options_description desc("Options");
        try
        {
            desc.add_options()
                ("help,h", "Display help messages")
                ("infile,i", po::value<std::string>(&m_InputFileName)->required(),
                             "Input image file to generate tensor from")
                ("model-format,f", po::value<std::string>(&m_ModelFormat)->required(),
                             "Format of the intended model file that uses the images."
                             "Different formats have different image normalization styles."
                             "Accepted values (caffe, tensorflow, tflite)")
                ("outfile,o", po::value<std::string>(&m_OutputFileName)->required(),
                             "Output raw tensor file path")
                ("output-type,z", po::value<std::string>(&m_OutputType)->default_value("float"),
                             "The data type of the output tensors."
                             "If unset, defaults to \"float\" for all defined inputs. "
                             "Accepted values (float, int or qasymm8)")
                ("new-width", po::value<std::string>(&m_NewWidth)->default_value("0"),
                             "Resize image to new width. Keep original width if unspecified")
                ("new-height", po::value<std::string>(&m_NewHeight)->default_value("0"),
                             "Resize image to new height. Keep original height if unspecified")
                ("layout,l", po::value<std::string>(&m_Layout)->default_value("NHWC"),
                             "Output data layout, \"NHWC\" or \"NCHW\", default value NHWC");
        }
        catch (const std::exception& e)
        {
            std::cerr << "Fatal internal error: [" << e.what() << "]" << std::endl;
            return false;
        }

        po::variables_map vm;

        try
        {
            po::store(po::parse_command_line(argc, argv, desc), vm);

            if (vm.count("help"))
            {
                std::cout << desc << std::endl;
                return false;
            }

            po::notify(vm);
        }
        catch (const po::error& e)
        {
            std::cerr << e.what() << std::endl << std::endl;
            std::cerr << desc << std::endl;
            return false;
        }

        if (!ValidateInputFile(m_InputFileName))
        {
            return false;
        }

        if (!ValidateLayout(m_Layout))
        {
            return false;
        }

        if (!ValidateOutputFile(m_OutputFileName))
        {
            return false;
        }

        return true;
    }

    std::string GetInputFileName() {return m_InputFileName;}
    armnn::DataLayout GetLayout()
    {
        if (m_Layout == "NHWC")
        {
            return armnn::DataLayout::NHWC;
        }
        else if (m_Layout == "NCHW")
        {
            return armnn::DataLayout::NCHW;
        }
        else
        {
            throw armnn::Exception("Unsupported data layout: " + m_Layout);
        }
    }
    std::string GetOutputFileName() {return m_OutputFileName;}
    unsigned int GetNewWidth() {return static_cast<unsigned int>(std::stoi(m_NewWidth));}
    unsigned int GetNewHeight() {return static_cast<unsigned int>(std::stoi(m_NewHeight));}
    SupportedFrontend GetModelFormat()
    {
        if (m_ModelFormat == "caffe")
        {
            return SupportedFrontend::Caffe;
        }
        else if (m_ModelFormat == "tensorflow")
        {
            return SupportedFrontend::TensorFlow;
        }
        else if (m_ModelFormat == "tflite")
        {
            return SupportedFrontend::TFLite;
        }
        else
        {
            throw armnn::Exception("Unsupported model format" + m_ModelFormat);
        }
    }
    armnn::DataType GetOutputType()
    {
        if (m_OutputType == "float")
        {
            return armnn::DataType::Float32;
        }
        else if (m_OutputType == "int")
        {
            return armnn::DataType::Signed32;
        }
        else if (m_OutputType == "qasymm8")
        {
            return armnn::DataType::QAsymmU8;
        }
        else
        {
            throw armnn::Exception("Unsupported input type" + m_OutputType);
        }
    }

private:
    std::string m_InputFileName;
    std::string m_Layout;
    std::string m_OutputFileName;
    std::string m_NewWidth;
    std::string m_NewHeight;
    std::string m_ModelFormat;
    std::string m_OutputType;
};

} // namespace anonymous

int main(int argc, char* argv[])
{
    CommandLineProcessor cmdline;
    if (!cmdline.ProcessCommandLine(argc, argv))
    {
        return -1;
    }
    const std::string imagePath(cmdline.GetInputFileName());
    const std::string outputPath(cmdline.GetOutputFileName());
    const SupportedFrontend& modelFormat(cmdline.GetModelFormat());
    const armnn::DataType outputType(cmdline.GetOutputType());
    const unsigned int newWidth  = cmdline.GetNewWidth();
    const unsigned int newHeight = cmdline.GetNewHeight();
    const unsigned int batchSize = 1;
    const armnn::DataLayout outputLayout(cmdline.GetLayout());

    using TContainer = boost::variant<std::vector<float>, std::vector<int>, std::vector<uint8_t>>;
    std::vector<TContainer> imageDataContainers;
    const NormalizationParameters& normParams = GetNormalizationParameters(modelFormat, outputType);
    try
    {
        switch (outputType)
        {
            case armnn::DataType::Signed32:
                imageDataContainers.push_back(PrepareImageTensor<int>(
                    imagePath, newWidth, newHeight, normParams, batchSize, outputLayout));
                break;
            case armnn::DataType::QAsymmU8:
                imageDataContainers.push_back(PrepareImageTensor<uint8_t>(
                    imagePath, newWidth, newHeight, normParams, batchSize, outputLayout));
                break;
            case armnn::DataType::Float32:
            default:
                imageDataContainers.push_back(PrepareImageTensor<float>(
                    imagePath, newWidth, newHeight, normParams, batchSize, outputLayout));
                break;
        }
    }
    catch (const InferenceTestImageException& e)
    {
        ARMNN_LOG(fatal) << "Failed to load image file " << imagePath << " with error: " << e.what();
        return -1;
    }

    std::ofstream imageTensorFile;
    imageTensorFile.open(outputPath, std::ofstream::out);
    if (imageTensorFile.is_open())
    {
        boost::apply_visitor([&imageTensorFile](auto&& imageData) { WriteImageTensorImpl(imageData, imageTensorFile); },
                             imageDataContainers[0]);
        if (!imageTensorFile)
        {
            ARMNN_LOG(fatal) << "Failed to write to output file" << outputPath;
            imageTensorFile.close();
            return -1;
        }
        imageTensorFile.close();
    }
    else
    {
        ARMNN_LOG(fatal) << "Failed to open output file" << outputPath;
        return -1;
    }

    return 0;
}
