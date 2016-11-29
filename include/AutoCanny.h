#ifndef AUTOCANNY_H
#define AUTOCANNY_H

#include <opencv2/imgproc/imgproc.hpp>

using namespace cv;


//#include <fstream>

const double percent_of_pixels_not_edges = 0.7;     // MATLAB: Used for selecting thresholds
const double threshold_ratio             = 0.4;     // MATLAB: Low threshold is this fraction of the high one
const double sigma                       = 1.4142;  // MATLAB: 1-D Gaussian filter standard deviation


void CumSum(const Mat & histogram, Mat & cumsum)
{
    cumsum = Mat::zeros(histogram.rows, histogram.cols, histogram.type());

    cumsum.at<float>(0) = histogram.at<float>(0);
    for (int i = 1; i < histogram.total(); ++i)
    {
        cumsum.at<float>(i) = cumsum.at<float>(i - 1) + histogram.at<float>(i);
    }
}


void ImgGradient(const Mat & src, Mat & GX, Mat & GY)
{
    const int filter_length = 8 * static_cast<int>(ceil(sigma));
    const int n             = (filter_length - 1)/2;
    Mat gauss_filter_transpose;
    Mat gradient_gauss_filter_transpose;

    Mat gauss_filter = getGaussianKernel(filter_length, sigma);
    normalize(gauss_filter, gauss_filter, 1.0, 0.0, NORM_L1);

    Mat gradient_gauss_filter(gauss_filter.size(), gauss_filter.type());
    gradient_gauss_filter.at<double>(0)               = gauss_filter.at<double>(1) - gauss_filter.at<double>(0);
    gradient_gauss_filter.at<double>(filter_length-1) = -gradient_gauss_filter.at<double>(0);
    for (size_t i = 1; i < filter_length/2; ++i)
    {
        gradient_gauss_filter.at<double>(i)                 = (gauss_filter.at<double>(i+1) - gauss_filter.at<double>(i-1))/2.0;
        gradient_gauss_filter.at<double>(filter_length-1-i) = -gradient_gauss_filter.at<double>(i);
    }
    normalize(gradient_gauss_filter, gradient_gauss_filter, 2.0, 0.0, NORM_L1);

    flip(gauss_filter,          gauss_filter,          0);
    flip(gradient_gauss_filter, gradient_gauss_filter, 0);

    transpose(gauss_filter,          gauss_filter_transpose);
    transpose(gradient_gauss_filter, gradient_gauss_filter_transpose);

    filter2D(src, GX, CV_32F, gauss_filter,                    Point(0, n), 0, BORDER_REPLICATE);
    filter2D(GX,  GX, CV_32F, gradient_gauss_filter_transpose, Point(n, 0), 0, BORDER_REPLICATE);

    filter2D(src, GY, CV_32F, gauss_filter_transpose,          Point(n, 0), 0, BORDER_REPLICATE);
    filter2D(GY,  GY, CV_32F, gradient_gauss_filter ,          Point(0, n), 0, BORDER_REPLICATE);
}


void AutoThreshold(const Mat & GX, const Mat & GY, double & high, double & low)
{
    Mat              mag_gradient;
    Mat              mag_hist;
    Mat              mag_hist_cumsum;
    const int        channels[]   = {0, 1, 2};
    const int        hist_size[]  = {64};
    const float      range_r[]    = {0.0, 255.01};
    const float      range_g[]    = {0.0, 255.01};
    const float      range_b[]    = {0.0, 255.01};
    const float    * hist_range[] = {range_r, range_g, range_b};
    const double     percentile   = percent_of_pixels_not_edges * GX.cols * GX.rows;


    /* Hypot intensity gradient computation [1] */
    /* [1] https://en.wikipedia.org/wiki/Hypot  */
    Mat GX_abs = abs(GX);
    Mat GY_abs = abs(GY);
    Mat G_t    = min(GX_abs, GY_abs);
    Mat G_x    = max(GX_abs, GY_abs);
    divide(G_t, G_x, G_t);
    pow(G_t, 2.0, G_t);
    sqrt(Scalar(1.0, 1.0, 1.0) + G_t, mag_gradient);
    multiply(G_x, mag_gradient, mag_gradient);

    calcHist(&mag_gradient, 1, channels, Mat(), mag_hist, 1, hist_size, hist_range);

    CumSum(mag_hist, mag_hist_cumsum);
    
    MatIterator_<float> up_bgr;
    up_bgr = std::upper_bound(mag_hist_cumsum.begin<float>(), mag_hist_cumsum.end<float>(), percentile);
    high = (static_cast<double>(up_bgr.lpos()) + 1.0) / static_cast<double>(hist_size[0]) * 255.0;
    low  = threshold_ratio * high;
}


void AutoCanny(const Mat & src, Mat & dst)
{
    Mat GX;
    Mat GY;
    double high_threshold;
    double low_threshold;


    ImgGradient(src, GX, GY);

    AutoThreshold(GX, GY, high_threshold, low_threshold);

    /* OpenCV Canny edge of an image using custom image gradient [1] */
    /* [1] http://docs.opencv.org/master/dd/d1a/group__imgproc__feature.html#ga2a671611e104c093843d7b7fc46d24af */
    GX.convertTo(GX, CV_16SC3);
    GY.convertTo(GY, CV_16SC3);
    Canny(GX, GY, dst, low_threshold, high_threshold, true);
}


void AutoDirCanny(const Mat & src, Mat & dst)
{
    Mat GX;
    Mat GY;
    Mat dir;
    Mat dst_cart;
    double high_threshold;
    double low_threshold;


    ImgGradient(src, GX, GY);

    AutoThreshold(GX, GY, high_threshold, low_threshold);

    /* OpenCV Canny directed edge of an image using custom image gradient [1] */
    /* [1] http://docs.opencv.org/master/dd/d1a/group__imgproc__feature.html#ga2a671611e104c093843d7b7fc46d24af */
    Mat GX_avg(GX.rows, GX.cols, CV_32FC1);
    Mat GY_avg(GY.rows, GY.cols, CV_32FC1);
    if (GX.channels() > 1)
    {
//        Mat GX_avg(GX.rows, GX.cols, CV_32FC1);
//        Mat GY_avg(GY.rows, GY.cols, CV_32FC1);
        for (size_t i = 0; i < GX.rows; ++i)
        {
            for (size_t j = 0; j < GX.cols; ++j)
            {
                GX_avg.at<float>(i, j) = (GX.at<Vec3f>(i, j)[0] + GX.at<Vec3f>(i, j)[1] + GX.at<Vec3f>(i, j)[2]) / 3.0;
                GY_avg.at<float>(i, j) = (GY.at<Vec3f>(i, j)[0] + GY.at<Vec3f>(i, j)[1] + GY.at<Vec3f>(i, j)[2]) / 3.0;
            }
        }
        phase(GX_avg, GY_avg, dir);
    }
    else phase(GX, GY, dir);

    GX.convertTo(GX, CV_16SC3);
    GY.convertTo(GY, CV_16SC3);
    Canny(GX, GY, dst_cart, low_threshold, high_threshold, true);

//    GaussianBlur(dst_cart, dst_cart, Size(3, 3), 1, 0, BORDER_CONSTANT);

//    normalize(dst_cart, dst_cart, 0.0, 1.0, NORM_MINMAX);
    dir.copyTo(dst, dst_cart);

//    std::ofstream file_src;
//    std::ofstream file_GX;
//    std::ofstream file_GY;
//    std::ofstream file_GX_avg;
//    std::ofstream file_GY_avg;
//    std::ofstream file_dir;
//    std::ofstream file_edge;
//    std::ofstream file_edgedir;
//
//    file_src.open("./src.m");
//    file_GX.open("./gx.m");
//    file_GY.open("./gy.m");
//    file_GX_avg.open("./gx_avg.m");
//    file_GY_avg.open("./gy_avg.m");
//    file_dir.open("./dirang.m");
//    file_edge.open("./edge.m");
//    file_edgedir.open("./edgedir.m");
//
//    file_src << "src = " << src << ";";
//    file_GX << "GX = " << GX << ";";
//    file_GY << "GY = " << GY << ";";
//    file_GX_avg << "GX_avg = " << GX_avg << ";";
//    file_GY_avg << "GY_avg = " << GY_avg << ";";
//    file_dir << "dir_ang = " << dir << ";";
//    file_edge << "EC = " << dst_cart << ";";
//    file_edgedir << "ECDIR = " << dst << ";";
//
//    file_src.close();
//    file_GX.close();
//    file_GY.close();
//    file_GX_avg.close();
//    file_GY_avg.close();
//    file_dir.close();
//    file_edge.close();
//    file_edgedir.close();
}

#endif /* AUTOCANNY_H */
