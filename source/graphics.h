#include "OCRS.h"
#include "OcrUtils.h"
#include "Params.h"
#include "logger.h"  // NOLINT
#include "lunasvg.h"

using namespace lunasvg;
#if defined(_WIN32)
    #include "StringCoder.hpp"
    #include <direct.h>
    #include <io.h>
    #include <codecvt>
    #include <locale>
    #define GETCWD _getcwd
    #define ACCESS _access
    #define MKDIR(path) _mkdir(path)
#else  // MacOS/UNIX
    #include <dirent.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #define GETCWD getcwd
    #define ACCESS access
    #define MKDIR(path) mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH)
#endif

// Define maximum path length
#define MAX_PATH_LEN 512


bool dir_exists(const std::string name) {
  if (ACCESS(name.c_str(), 0) == 0) return true;
  return false;
}

// ref: https://blog.csdn.net/sinat_28327447/article/details/106113341
int32_t create_mul_dir(const std::string& dir) {
  uint32_t dirPathLen = dir.length();
  if (dirPathLen > MAX_PATH_LEN) {
    return -1;
  }
  char tmpDirPath[MAX_PATH_LEN] = {0};
  for (uint32_t i = 0; i < dirPathLen; ++i) {
    tmpDirPath[i] = dir[i];
    if (tmpDirPath[i] == '\\' || tmpDirPath[i] == '/') {
      if (ACCESS(tmpDirPath, 0) != 0) {
        int32_t ret = MKDIR(tmpDirPath);
        if (ret != 0) return ret;
      }
    }
  }
  return 0;
}

int make_dir_from_dir(const std::string dir) {
  if (create_mul_dir(dir + "/") != 0) return false;
  return true;
}


cv::Mat render_svg_to_opencv(const std::string& svg_filename) {
#ifdef _WIN32
    // 将 UTF-8 路径转换为 UTF-16（Windows 使用宽字符路径）
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring wstr = converter.from_bytes(svg_filename);

    // 打开文件（使用 std::ifstream 并以二进制模式读取）
    std::ifstream file(wstr, std::ios::binary);
    if (!file) {
        return cv::Mat();
    }

    // 获取文件内容的大小
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // 读取文件内容到内存
    std::vector<char> file_data(file_size);
    file.read(file_data.data(), file_size);

    // 将文件内容传递给 LunaSVG 来加载文档
    auto document = lunasvg::Document::loadFromData(file_data.data(), file_size);
#else
    auto document = lunasvg::Document::loadFromFile(svg_filename.c_str());
#endif
    // 如果文档加载失败，返回空的 cv::Mat
    if (document == nullptr) {
        return cv::Mat();
    }

    // 渲染成位图
    auto bitmap = document->renderToBitmap();
    if (bitmap.width() == 0 || bitmap.height() == 0) {
        return cv::Mat();
    }

    // 获取位图的宽度和高度
    int width = bitmap.width();
    int height = bitmap.height();

    // 获取位图的数据指针（RGBA 格式）
    const uint8_t* img_data = bitmap.data();

    // 创建 OpenCV Mat 对象，用于存储 RGBA 图像数据
    cv::Mat img(height, width, CV_8UC4, const_cast<uint8_t*>(img_data));

    cv::Mat img_bgr;
    cv::cvtColor(img, img_bgr, cv::COLOR_RGBA2BGR);

    return img_bgr;
}


cv::Mat readAndDecodeImage(const std::string& filename, bool isSVGFile) {
    // 检查文件扩展名

    if (isSVGFile) {
        return render_svg_to_opencv(filename.c_str());
    } else {
#ifdef _WIN32
        // 对其他格式，使用原有逻辑
        // Convert filename from UTF-8 to UTF-16
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wstr = converter.from_bytes(filename);

        // Open the file
        std::ifstream file(wstr, std::ios::binary);

        // Read the file data into a vector
        std::vector<uchar> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        // Decode the image
        cv::Mat img_decode = cv::imdecode(data, cv::IMREAD_COLOR);

        return img_decode;
#else
        cv::Mat input_img = cv::imread(
            filename, cv::IMREAD_COLOR);  // default : BGRcv::IMREAD_COLOR
        return input_img;
#endif
      }
}


int OCRS:: GetImage(const std::string& filename, cv::Mat* result, const pOCRS& params, bool isSVG) {
  cv::Mat input_img;  // 全局使用
  // 图片读取异常处理
  try {
    input_img = readAndDecodeImage(filename, isSVG);

    // filter image size
    if (input_img.cols > params.image_limit_max && input_img.rows > params.image_limit_max) {
        // 计算图像的面积
        float image_area = input_img.cols * input_img.rows;
        
        // 计算最大允许面积
        float max_area = params.image_limit_max * params.image_limit_max;

        // 计算权重，图像越大，权重越大，比例越小
        float weight = std::max(1.0f, std::sqrt(image_area / max_area));

        // 根据权重计算缩放比例
        float ratio = 1.0f / weight; 

        // 调整图像大小
        cv::resize(input_img, input_img, cv::Size(input_img.cols * ratio, input_img.rows * ratio));
    }
    if (input_img.cols + input_img.rows < 128) {
        float scale_factor = 128.0f / std::max(input_img.cols, input_img.rows);
        cv::resize(input_img, input_img, cv::Size(input_img.cols * scale_factor, input_img.rows * scale_factor));
    }
  } catch (const std::exception& e) {
    INFO(logger, "%s Read Fails! <error: %s", filename.c_str(), e.what());
    return FILE_NOT_READ;
  }
  *result = input_img;
  return OCRS_OK;
}

// OCR整体初始化
bool OCRS::OCRSInit(const pOCRS& params) {
  int retCode;
  this->ocrs_params_ = params;

  if (params.enable_SM || params.enable_SR) {
    // yolov5
    det_seal_param_ = params.det_seal_param;
#ifdef __ANDROID__
    det_seal_param_.mgr = mgr;
#endif
    Det_Seal_.visual = params.visual;
    Det_Seal_.inter_threads = params.inter_threads;
    Det_Seal_.outer_threads = params.outer_threads;
    Det_Seal_.logger = logger;
    // yolov5 加载模型
    retCode = Det_Seal_.initModel(det_seal_param_);
    if (retCode != OCRS_OK) {
      WARN(logger, "Init Seal Dectection Model Failed(%s), Error Message: %s .",
           det_seal_param_.model_path.c_str(), errocrcode_map[retCode].str);
      det_seal_init_ = false;
    } else {
      // flag of initialization
      det_seal_init_ = true;
    }

    if (params.enable_SM && det_seal_init_) {
      // MATCH 印章匹配
      match_seal_param_ = params.match_seal_param;
#ifdef __ANDROID__
      match_seal_param_.mgr = mgr;
      match_seal_param_.app_path = app_path;
#endif
      Match_Seal_.visual = params.visual;
      Match_Seal_.inter_threads = params.inter_threads;
      Match_Seal_.outer_threads = params.outer_threads;
      Match_Seal_.logger = logger;
      retCode = Match_Seal_.initModel(match_seal_param_);
      if (retCode != OCRS_OK) {
        WARN(logger,
             "Init Seal Match Directory Failed(%s), Error Message: "
             "%s .",
             match_seal_param_.templates_dir.c_str(),
             errocrcode_map[retCode].str);
        match_seal_init_ = false;
      } else {
        match_seal_init_ = true;
      }
    }

    if (params.enable_SR && det_seal_init_) {
      // seal_dbnet 印章检测
      det_seal_text_param_ = params.det_seal_text_param;
#ifdef __ANDROID__
      det_seal_text_param_.mgr = mgr;
#endif
      Det_Seal_Text_.visual = params.visual;
      Det_Seal_Text_.inter_threads = params.inter_threads;
      Det_Seal_Text_.outer_threads = params.outer_threads;
      Det_Seal_Text_.logger = logger;
      // 印章文本检测
      retCode = Det_Seal_Text_.initModel(det_seal_text_param_); //DbNet
      if (retCode != OCRS_OK) {
        WARN(logger,
             "Init Seal Text Dectection Model Failed(%s), Error Message: "
             "%s .",
             det_seal_text_param_.model_path.c_str(),
             errocrcode_map[retCode].str);

        det_seal_text_init_ = false;
      } else {
        det_seal_text_init_ = true;
      }
    }
  }
  if (params.enable_OCR) {
    // text_dbnet
    det_common_text_param_ = params.det_common_text_param;
#ifdef __ANDROID__
    det_common_text_param_.mgr = mgr;
#endif
    Det_Common_Text_.visual = params.visual;
    Det_Common_Text_.inter_threads = params.inter_threads;
    Det_Common_Text_.outer_threads = params.outer_threads;
    Det_Common_Text_.logger = logger;
    retCode = Det_Common_Text_.initModel(det_common_text_param_);
    if (retCode != OCRS_OK) {
      WARN(logger,
           "Init Common Text Dectection Model Failed(%s), Error Message: "
           "%s .",
           det_common_text_param_.model_path.c_str(),
           errocrcode_map[retCode].str);
      det_common_text_init_ = false;
    } else {
      det_common_text_init_ = true;
    }
  }
  if ((params.enable_OCR && det_common_text_init_) ||
      (params.enable_SR && det_seal_text_init_)) {
    if (params.enable_det_angle) {
      // angel
      det_angle_param_ = params.det_angle_param;
#ifdef __ANDROID__
      det_angle_param_.mgr = mgr;
#endif
      Det_Angle_.visual = params.visual;
      Det_Angle_.inter_threads = params.inter_threads;
      Det_Angle_.outer_threads = params.outer_threads;
      Det_Angle_.logger = logger;
      retCode = Det_Angle_.initModel(det_angle_param_);
      if (retCode != OCRS_OK) {
        WARN(logger,
             "Init Common Text Dectection Model Failed(%s), Error Message: "
             "%s .",
             det_angle_param_.model_path.c_str(), errocrcode_map[retCode].str);
        det_angle_init_ = false;
      } else {
        det_angle_init_ = true;
      }
    }

    // crnn
    rec_text_param_ = params.rec_text_param;
#ifdef __ANDROID__
    rec_text_param_.mgr = mgr;
#endif
    Rec_Text_.visual = params.visual;
    Rec_Text_.inter_threads = params.inter_threads;
    Rec_Text_.outer_threads = params.outer_threads;
    Rec_Text_.logger = logger;
    retCode = Rec_Text_.initModel(rec_text_param_);
    if (retCode != OCRS_OK) {
      WARN(logger,
           "Init Text Recognition Model Failed(%s, %s), Error Message: %s .",
           rec_text_param_.model_path.c_str(),
           rec_text_param_.label_path.c_str(), errocrcode_map[retCode].str);
      rec_text_init_ = false;
    } else {
      rec_text_init_ = true;
    }
  }

  if (params.enable_OCR &&
      ((det_common_text_init_ && rec_text_init_) || det_angle_init_)) {
    OCR_ = true;
  }

  if (params.enable_SM && (match_seal_init_ && det_seal_init_)) {
    SM_ = true;
  }

  if (params.enable_SR &&
      ((det_seal_text_init_ && rec_text_init_) || det_angle_init_)) {
    SR_ = true;
  }
  return true;
}

// OCR功能初始化
bool OCRS::OCRSInits(std::string workDir, const char* configPath) {
  double begin = getCurrentTime();
  // task 0:ocr 1:seal
  // 绝对路径/ 相对于可执行文件的相对路径
  std::string confPath = configPath;
  if (!isFileExists(confPath)) {
    WARN(logger,
         "Init Models Failed,  Failed to Read the Path of "
         "Configuration(%s)!",
         confPath.c_str());
    return 0;
  }
  std::string configInfo;
  try {
    Config config = Config(workDir + SEG); //通过.dlpcfg配置
    config.loadConfig(confPath, workDir + SEG);
    configInfo = config.PrintConfigInfo();
    ocrs_params_ = config.ocrs;
  } catch (const std::exception& e) {
    WARN(logger, "Parse configuration Failed, Error: %s", e.what());
  }

  if (ocrs_params_.visual) {
    visual_path = workDir + SEG + "Bak";
    if (!dir_exists(visual_path)) {
      make_dir_from_dir(visual_path);
    }
  }
  if (!this->OCRSInit(ocrs_params_)) {
    INFO(logger, "Init Models Failed, Cost Time : %.3fms.\n%s",
         getCurrentTime() - begin, configInfo.c_str());
    return false;
  }

  INFO(logger,
       "Init Models(input/init) [OCR: %d/%d , SR: %d/%d , SM: %d/%d] "
       "Success, Cost Time : %.3fms.\n%s",
       ocrs_params_.enable_OCR, OCR_, ocrs_params_.enable_SR, SR_,
       ocrs_params_.enable_SM, SM_, getCurrentTime() - begin,
       configInfo.c_str());

  initialized_ = OCR_ || SM_ || SR_;
  if (initialized_)
    return true;
  else
    return false;
}

// 安卓 OCR功能初始化
#ifdef __ANDROID__
bool OCRS::OCRSInits() {
  double begin = getCurrentTime();
  // task 0:ocr 1:seal
  const char* confPath = "dlpocr.cfg";
  //    AAssetManager* mgr = AAssetManager_fromJava(jniEnv, assetManager);

  AAsset* asset = AAssetManager_open(mgr, confPath, AASSET_MODE_BUFFER);
  if (!asset) {
    // 处理文件打开失败的情况
    return true;
  }

  const void* data = AAsset_getBuffer(asset);
  off_t size = AAsset_getLength(asset);

  // 将文件内容读取到缓冲区中
  char* buffer = new char[size + 1];
  memcpy(buffer, data, size);
  buffer[size] = '\0';

  // 关闭文件
  AAsset_close(asset);

  //    if (!isFileExists(confPath)) {
  //        // WARN(logger,
  //             "Init Models Failed,  Failed to Read the Path of "
  //             "Configuration(%s)!",
  //             confPath.c_str());
  //        return 0;
  //    }
  std::string configInfo;
  try {
#ifdef __ANDROID__
    Config config = Config(buffer, "");
#else
    Config config = Config(buffer);
#endif
    // 释放缓冲区内存
    delete[] buffer;
    configInfo = config.PrintConfigInfo();
    ocrs_params_ = config.ocrs;
  } catch (const std::exception& e) {
    WARN(logger, "Parse configuration Failed, Error: %s", e.what());
  }
#ifndef __ANDROID__
  if (ocrs_params_.visual) {
    visual_path = "Bak";
    if (!dir_exists(visual_path)) {
      make_dir_from_dir(visual_path);
    }
  }
#endif
  if (!this->OCRSInit(ocrs_params_)) {
    INFO(logger, "Init Models Failed, Cost Time : %.3fms.\n%s",
         getCurrentTime() - begin, configInfo.c_str());
    return false;
  }

  INFO(logger,
       "Init Models(input/init) [OCR: %d/%d , SR: %d/%d , SM: %d/%d] "
       "Success, Cost Time : %.3fms.\n%s",
       ocrs_params_.enable_OCR, OCR_, ocrs_params_.enable_SR, SR_,
       ocrs_params_.enable_SM, SM_, getCurrentTime() - begin,
       configInfo.c_str());

  initialized_ = OCR_ || SM_ || SR_;
  if (initialized_)
    return true;
  else
    return false;
}
#endif

template <typename T>
void write_to_map(std::map<std::string, std::vector<T>>& data_map,  // NOLINT
                  const std::string& key, const T& data) {
  auto iter = data_map.find(key);
  if (iter == data_map.end()) {
    data_map.insert(std::make_pair(key, std::vector<T>()));
    iter = data_map.find(key);
  }
  iter->second.push_back(data);
}

// 文本框检测
int OCRS::TextDetRun(
    DbNet& Det_Text_, pDbnet& det_text_param_, const cv::Mat& img,
    const std::string& ocr_type, const int& cnt,
    std::map<std::string, std::vector<std::vector<cv::Mat>>>& all_images,
    std::map<std::string, std::vector<std::vector<std::vector<int>>>>& all_linecnt) {

  double begin = getCurrentTime();
  std::vector<TextBox> box;
  cv::Mat det_text_out_img;

  int retCode = Det_Text_.Run(img, det_text_param_, &det_text_out_img, &box);//文本检测
  if (retCode != OCRS_OK) {
    WARN(logger,
         "%s Image.%d,Text Dectection Run Failed, Error "
         "Message: %s.",
         ocr_type.c_str(), cnt, errocrcode_map[retCode].str);
    return OCRS_FAIL;
  }
  if (isOutTime(startTime, limitTime)) {
    return OCRS_TIMEOUT;
  }
  INFO(logger,
       "%s Image.%d, Text Detection, Get %d Text Boxes, "
       "Cost Time: %.3fms.",
       ocr_type.c_str(), cnt, box.size(), getCurrentTime() - begin);
  if (!box.empty()) {
    std::vector<std::vector<int>> linecnt_new;  // 文本行
    std::vector<cv::Mat> partImages(box.size());
    begin = getCurrentTime();
    // 后处理 - 获取文本区域的图片
    postprocess(det_text_out_img, box, partImages, linecnt_new);
    INFO(logger,
         "%s Image.%d, Text Postprocess, Get %d Lines, Cost "
         "Time: %.3fms.",
         ocr_type.c_str(), cnt, linecnt_new.size(), getCurrentTime() - begin);
    write_to_map(all_images, ocr_type, partImages);
    write_to_map(all_linecnt, ocr_type, linecnt_new);
  }
  return OCRS_OK;
}

#if defined(__GNUC__)
std::string normalize_newlines(const std::string& input) {
    std::string output = input;

    // 1. 替换所有的 \r\n 为 \n
    size_t pos = 0;
    while ((pos = output.find("\r\n", pos)) != std::string::npos) {
        output.replace(pos, 2, "\n");
        pos++;  // 移动到下一个位置
    }

    // 2. 去掉所有剩余的 \r
    pos = 0;
    while ((pos = output.find('\r', pos)) != std::string::npos) {
        output.erase(pos, 1);
    }

    return output;
}
#endif

// 分割图片的函数
std::vector<cv::Mat> splitImageByAspectRatio(const cv::Mat& image, double maxAspectRatio) {
    std::vector<cv::Mat> splitImages;

    // 获取图片宽高
    int height = image.rows;
    int width = image.cols;

    // 计算高宽比
    double aspectRatio = static_cast<double>(height) / static_cast<double>(width);

    // 如果高宽比小于阈值，不需要分割，直接返回原图
    if (aspectRatio <= maxAspectRatio) {
        splitImages.push_back(image);
        return splitImages;
    }

    // 计算需要分割的数量
    int numSplits = static_cast<int>(std::ceil(aspectRatio / maxAspectRatio));
    int splitHeight = height / numSplits;  // 每部分的高度

    // 分割图片
    for (int i = 0; i < numSplits; ++i) {
        int startY = i * splitHeight;
        int endY = (i == numSplits - 1) ? height : (i + 1) * splitHeight; // 最后一部分可能不均匀
        cv::Rect region(0, startY, width, endY - startY);
        splitImages.push_back(image(region));
    }

    return splitImages;
}

int OCRS::OCRSRuns(
    const cv::Mat& in_img, const pOCRS& params,
    std::map<std::string, std::vector<std::string>>& result) {  // NOLINT
  int retCode;
  double begin;
  double ocr_start = getCurrentTime();
  // 检测的图片
  std::map<std::string, std::vector<std::vector<cv::Mat>>> all_images;
  // 图片的位置信息，用于排序
  std::map<std::string, std::vector<std::vector<std::vector<int>>>> all_linecnts;

  std::vector<cv::Mat> srcImgs = splitImageByAspectRatio(in_img, 4.0);
  std::vector<CropInfo> det_areas_output;  // seal
  // 当开启匹配以及识别功能时，敏感区域检测模块必需启用
  if ((SM_ || SR_) && params.typeshow != "none") {
    // yolov5
    det_seal_param_ = params.det_seal_param;
    Det_Seal_.file_id = file_id + " - sr_sm";
    Det_Seal_.startTime = startTime;
    Det_Seal_.limitTime = limitTime;

    bool mask = (params.typeshow == "typecontent");

    begin = getCurrentTime();

    for (size_t i = 0; i < srcImgs.size(); ++i) {  // 遍历每个分割图像

        retCode = Det_Seal_.Run(srcImgs[i], det_seal_param_, &det_areas_output, mask);
        if (retCode != OCRS_OK) {
            ERROR(logger, "Detection Run Failed for segment %d, Error Message: %s.",
                  i, errocrcode_map[retCode].str);
            continue;  // 跳过当前分割部分，继续下一个
        }

        // yolov5 推理后超时
        if (isOutTime(startTime, limitTime)) {
            return OCRS_TIMEOUT;
        }
    }
    INFO(logger, "Detection, Get %d Areas, Cost Time: %.3fms.",
         det_areas_output.size(), getCurrentTime() - begin);
  }

  // 当有匹配内容
if (!det_areas_output.empty()) {
    std::unordered_set<std::string> unique_classes;
    int cnt = 0;

    for (const auto& area_info : det_areas_output) {
      std::string classStr = Det_Seal_.classNames[area_info.classId];
      unique_classes.insert(classStr);

      // 初始化结果map
      if (result.find(classStr) == result.end()) {
        result[classStr] = std::vector<std::string>();
      }

      // 处理类型展示
      if (params.typeshow == "type") {
        result[classStr].emplace_back("");
      }

      auto& img = area_info.img;
      
      // 图像匹配
      if (SM_ && SM_lists.find(classStr) != SM_lists.end()) {
        match_seal_param_ = params.match_seal_param;
        Match_Seal_.file_id = Det_Seal_.file_id + " - match_" + std::to_string(cnt + 1);
        Match_Seal_.startTime = startTime;
        Match_Seal_.limitTime = limitTime;

        int res_match;
        double begin = getCurrentTime();
        retCode = Match_Seal_.Run(img, match_seal_param_, &res_match);

        if (retCode != OCRS_OK) {
            WARN(logger, "%s Image.%d, Image Match Run Failed, Error Message: %s.",
                  classStr.c_str(), cnt, errocrcode_map[retCode].str);
            continue;
        }
        if (isOutTime(startTime, limitTime)) return OCRS_TIMEOUT;

        INFO(logger, "%s Image.%d, Image Match, Get %d Similar Seals, Cost Time: %.3fms.",
              classStr.c_str(), cnt, res_match, getCurrentTime() - begin);
      }

      // 文字识别
      if (SR_ && params.typeshow == "typecontent") {
        if (area_info.classId == 0 || area_info.classId == 1) {  // Circle or Oval
            det_seal_text_param_ = params.det_seal_text_param;
            Det_Seal_Text_.file_id = Det_Seal_.file_id + " - " + classStr + "_" + std::to_string(cnt + 1);
            Det_Seal_Text_.startTime = startTime;
            Det_Seal_Text_.limitTime = limitTime;
            retCode = TextDetRun(Det_Seal_Text_, det_seal_text_param_, img, classStr, cnt, all_images, all_linecnts);
        } else if (OCR_) {  // Other OCR Detection
            Det_Common_Text_.file_id = Det_Seal_.file_id + " - " + classStr + "_" + std::to_string(cnt + 1);
            Det_Common_Text_.startTime = startTime;
            Det_Common_Text_.limitTime = limitTime;
            retCode = TextDetRun(Det_Common_Text_, det_common_text_param_, img, classStr, cnt, all_images, all_linecnts);
        }

        if (retCode == OCRS_TIMEOUT) return OCRS_TIMEOUT;
        cnt++;
      }
    }

    // Logging detected classes
    std::string areas_class = "";
    for (const auto& class_name : unique_classes) {
        areas_class += class_name + "  ";
    }
    INFO(logger, "Detect Image, Get %d classes: %s.", unique_classes.size(), areas_class.c_str());
}

  if (OCR_) {
    // Common text detection
    std::string ocr_type = "OCR";
    Det_Common_Text_.file_id = Det_Seal_.file_id + " - " + ocr_type;
    Det_Common_Text_.startTime = startTime;
    Det_Common_Text_.limitTime = limitTime;

    for (size_t i = 0; i < srcImgs.size(); ++i) {  // 遍历每个分割图像

      retCode = TextDetRun(Det_Common_Text_, det_common_text_param_, srcImgs[i], ocr_type, i, all_images, all_linecnts);
      if (retCode != OCRS_OK) {
          ERROR(logger, "Text Detection Run Failed, Error Message: %s.",
                i, errocrcode_map[retCode].str);
          continue;  // 跳过当前分割部分，继续下一个
      }
    }
  }

  // 文字角度检测以及识别
  if (OCR_ || SR_) {
    // 设置参数
    if (params.enable_det_angle) det_angle_param_ = params.det_angle_param;
    rec_text_param_ = params.rec_text_param;

    // 遍历 all_images
    for (auto& pair : all_images) {
      std::string classStr = pair.first;
      auto& imageGroups = pair.second;
      double crnn_start_time = getCurrentTime();

      int total_characters = 0;
      std::string aggregated_text_result("");

      for (size_t i = 0; i < imageGroups.size(); ++i) {
        std::vector<cv::Mat>& partImages = imageGroups[i];
        std::vector<std::vector<int>>& linecnt = all_linecnts[classStr][i];
        std::string text_result;

        // 角度检测
        if (det_angle_init_) {
            Det_Angle_.file_id = file_id + " - angle_" + std::to_string(i);
            Det_Angle_.startTime = startTime;
            Det_Angle_.limitTime = limitTime;

            retCode = Det_Angle_.Run(partImages, det_angle_param_);
            if (retCode != OCRS_OK) {
                WARN(logger, "Angle Detection Failed for Class %s, Error Message: %s.",
                    classStr.c_str(), errocrcode_map[retCode].str);
                continue;
            }

            if (isOutTime(startTime, limitTime)) {
                return OCRS_TIMEOUT;
            }
        }

        // 文本识别
        Rec_Text_.file_id = file_id + " - crnn_" + std::to_string(i);
        Rec_Text_.startTime = startTime;
        Rec_Text_.limitTime = limitTime;

        retCode = Rec_Text_.Run(partImages, linecnt, &text_result);
        if (retCode != OCRS_OK) {
            WARN(logger, "Text Recognition Failed for Class %s, Error Message: %s.",
                classStr.c_str(), errocrcode_map[retCode].str);
            continue;
        }

        if (isOutTime(startTime, limitTime)) {
            return OCRS_TIMEOUT;
        }

        // 处理文本结果
#if defined(__GNUC__)
        text_result = normalize_newlines(text_result);
#endif
        result[classStr].push_back(text_result);

        int char_count = getStrLenUtf8(text_result.c_str());
        total_characters += char_count;
        aggregated_text_result += text_result + "\n";
      }

      INFO(logger, "Text Recognization, Get %d Characters, Cost Time: %.3fms.",
              total_characters, getCurrentTime() - crnn_start_time);

      DEBUG(logger, "Text Recognization Result for Developer:\n[%s]%s", classStr.c_str(),
              aggregated_text_result.c_str());
    }
  }

  INFO(logger, "OCRS TASK Finished, Total Cost Time: %.3fms.",
       getCurrentTime() - ocr_start);
  return OCRS_OK;
}

int OCRS::OCRSRuns(
    const std::string imgPath, const pOCRS& params,
    std::map<std::string, std::vector<std::string>>& ocr_result, bool isSVG) {  // NOLINT
  if (!initialized_) {
    return OCRS_ERROR;
  }
  std::string abspath = abs_path(imgPath);
  std::string filename =
      abspath.substr(abspath.find_last_of("\\/") + 1, abspath.rfind('.'));

  file_id = visual_path + SEG + filename.substr(0, filename.rfind("."));

  cv::Mat input_img;

  int retCode = GetImage(abspath, &input_img, params, isSVG);

  if (retCode != OCRS_OK) {
    WARN(logger, "Image Read Failed, Path: %s", abspath.c_str());
    return OCRS_ERROR;
  }

  INFO(logger, "Image Path: %s\nImage Size(whc): %d x %d x %d .",
       abspath.c_str(), input_img.rows, input_img.cols,
       input_img.channels());
  try {
    retCode = this->OCRSRuns(input_img, ocrs_params_, ocr_result);
  } catch (const std::exception& e) {
    WARN(logger, "OCRSRun Error: %s", e.what());
  }

  if (retCode == OCRS_TIMEOUT) {
    return OCRS_TIMEOUT;
  }
  return OCRS_OK;
}

std::vector<std::vector<int>> OCRS::getLinesIndex(const std::vector<TextBox>& textBoxes) {
    const int length = textBoxes.size();
    if (length == 0) {
        return {};
    }

    // 提前分配容器空间
    std::vector<cv::Point> centerPos(length);
    std::vector<int> heights(length);

    // 1. 计算每个文本框的中心点和高度
    for (size_t i = 0; i < length; ++i) {
        const auto& boxPoints = textBoxes[i].boxPoint;
        int xSum = 0, ySum = 0;
        int maxY = boxPoints[0].y, minY = boxPoints[0].y;

        for (const auto& point : boxPoints) {
            xSum += point.x;
            ySum += point.y;
            maxY = std::max(maxY, point.y);
            minY = std::min(minY, point.y);
        }

        centerPos[i] = cv::Point(xSum / boxPoints.size(), ySum / boxPoints.size());
        heights[i] = maxY - minY;  // 高度
    }

    // 2. 计算平均高度
    int average_h = 0;
    if (length > 10) {
        int total = std::accumulate(heights.begin(), heights.end(), 0);
        int maxH = *std::max_element(heights.begin(), heights.end());
        int minH = *std::min_element(heights.begin(), heights.end());
        average_h = (total - maxH - minH) / (length - 2);
    } else {
        average_h = std::accumulate(heights.begin(), heights.end(), 0) / length;
    }

    // 3. 按行分组
    int IErr = average_h;  // 同一行允许的误差
    std::vector<std::vector<cv::Point>> point_all;
    point_all.reserve(centerPos.size());  // 预分配空间

    // 将第一个点作为初始行
    point_all.push_back({centerPos[0]});

    for (size_t i = 1; i < centerPos.size(); ++i) {
        bool added = false;
        for (auto& row : point_all) {
            if (std::abs(centerPos[i].y - row[0].y) <= IErr) {
                row.push_back(centerPos[i]);
                added = true;
                break;
            }
        }
        if (!added) {
            point_all.push_back({centerPos[i]});
        }
    }

    // 4. 按列排序
    for (auto& row : point_all) {
        std::sort(row.begin(), row.end(), [](const cv::Point& p1, const cv::Point& p2) {
            return p1.x < p2.x;
        });
    }

    // 5. 将中心点映射为索引
    std::vector<std::vector<int>> licnt(point_all.size());
    for (size_t row = 0; row < point_all.size(); ++row) {
        licnt[row].resize(point_all[row].size());
        for (size_t col = 0; col < point_all[row].size(); ++col) {
            licnt[row][col] = std::distance(centerPos.begin(),
                                            std::find(centerPos.begin(), centerPos.end(), point_all[row][col]));
        }
    }

    return licnt;
}

void OCRS::postprocess(const cv::Mat& src,
                       const std::vector<TextBox>& textBoxes,
                       std::vector<cv::Mat>& partImages,          // NOLINT
                       std::vector<std::vector<int>>& linecnt) {  // NOLINT
  linecnt = getLinesIndex(textBoxes);
  for (int i = 0; i < textBoxes.size(); i++) {
    try {
      if (textBoxes.at(i).boxPoint.size() == 4) {
        // 矩形操作  这里会对位置作仿射变换
        partImages.at(i) = getRotateCropImage(src, textBoxes.at(i).boxPoint);
      } else {
        // 多边形操作
        partImages.at(i) = curv2rec(src, textBoxes.at(i).boxPoint);  // can't fit rec, fit ellipse
      }
      if (ocrs_params_.visual) {
        cv::Mat save_img;
        cv::cvtColor(partImages.at(i), save_img, cv::COLOR_BGR2RGB);
        cv::imwrite(file_id + " - crop_res_" + std::to_string(i) + ".png", save_img);
      }
    } catch (const std::exception& e) {
      WARN(logger, "Postprocess Failed! text index: %d, error: %s", i,
           e.what());
    }
  }
}
