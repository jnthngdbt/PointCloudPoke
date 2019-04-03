#include "VisualizerData.h"

#include <algorithm>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <chrono>
#include <sstream>

#include <boost/filesystem.hpp>

#include <pcl/io/pcd_io.h>

using namespace pcv;

const std::string VisualizerData::sFilePrefix = "visualizer.";
const std::string VisualizerData::sFolder = "VisualizerData/";

void logError(const std::string& msg)
{
    std::cout << "[VISUALIZER][ERROR]" << msg << std::endl;
}

void logWarning(const std::string& msg)
{
    std::cout << "[VISUALIZER][WARNING]" << msg << std::endl;
}

VisualizerData::VisualizerData(const std::string& name) : mName(name) { }

Cloud& VisualizerData::addFeature(const FeatureData& data, const FeatureName& featName, const CloudName& cloudName, ViewportIdx viewport)
{
    return getCloud(cloudName).addFeature(data, featName, viewport);
}

Cloud& VisualizerData::addLabelsFeature(const std::vector< std::vector<int> >& componentsIndixes, const FeatureName& featName, const CloudName& cloudName, ViewportIdx viewport)
{
    return getCloud(cloudName).addLabelsFeature(componentsIndixes, featName, viewport);
}

Cloud& VisualizerData::addSpace(const FeatureName& a, const FeatureName& b, const FeatureName& c, const CloudName& cloudName)
{
    return getCloud(cloudName).addSpace(a, b, c);
}

void VisualizerData::render()
{
    prepareCloudsForRender(mClouds);
}

void VisualizerData::prepareCloudsForRender(CloudsMap& clouds)
{
    for (auto& pair : clouds)
    {
        const auto& name = pair.first;
        auto& cloud = *pair.second;

        // TODO make generateAllPossibleGeoHandlers if no space defined
        if (cloud.mSpaces.size() == 0)
        {
            logError("[render] No space set for [" + name + "]. Must call addSpace().");
            continue;
        }

        if (cloud.mRGB.r >= 0.0)
        {
            // Create packed RGB value.
            const auto r = static_cast<uint8_t>(cloud.mRGB.r * 255);
            const auto g = static_cast<uint8_t>(cloud.mRGB.g * 255);
            const auto b = static_cast<uint8_t>(cloud.mRGB.b * 255);
            const auto rgb = static_cast<float>((r << 16) + (g << 8) + (b));
            cloud.addFeature(std::vector<float>(cloud.getNbPoints(), rgb), "rgb", cloud.mViewport);
        }

        // Some color and geometry handlers only work with PointCloud2 objects, 
        // and the best way to create them is by reading a file. That is nice, because
        // we want to save the file anyway, because allows to save in a single cloud
        // multiple custom features.
        pcl::PCLPointCloud2::Ptr pclCloudMsg(new pcl::PCLPointCloud2());
        if (CreateDirectoryA(sFolder.c_str(), NULL) || (ERROR_ALREADY_EXISTS == GetLastError())) // WARNING: Windows only. With c++17, use std::filesystem::create_directory.
        {
            const std::string fileName = getCloudFilename(cloud, name);
            cloud.save(fileName);
            pcl::io::loadPCDFile(fileName, *pclCloudMsg);
        }
        else
            logError("Could not create folder '" + sFolder + "', undefined behavior will follow.");
    }
}

Cloud& VisualizerData::getCloud(const CloudName& name)
{
    if (!mClouds[name])
    {
        mClouds[name].reset(new Cloud());
    }

    mClouds[name]->setParent(this);

    return *mClouds[name];
}

void VisualizerData::setFeaturesOrder(const std::vector<FeatureName>& names)
{
    logWarning("[VisualizerData::setFeaturesOrder] not implemented yet");
}

void VisualizerData::addBasis(
    const Eigen::Vector3f& u1,
    const Eigen::Vector3f& u2,
    const Eigen::Vector3f& u3,
    const Eigen::Vector3f& origin,
    const std::string& name,
    double scale,
    ViewportIdx viewport)
{
    logWarning("[VisualizerData::addBasis] not implemented yet");
}

void VisualizerData::clearSavedData(int lastHrsToKeep)
{
    namespace fs = boost::filesystem;

    if (!fs::exists(fs::path(sFolder)))
        return;

    const auto timeLimitBack = VisualizerData::createTimestampString(lastHrsToKeep);

    // Loop on files in export folder and delete old files.
    for (const auto file : fs::directory_iterator(fs::path(sFolder)))
    {
        const auto name = file.path().stem().string();
        
        // Only consider "visualizer.20**.****..." files. 
        if (name.substr(0, sFilePrefix.size() + 2) == (sFilePrefix + "20"))
        {
            const auto fileTime = name.substr(sFilePrefix.size(), 19); // YYYYMMDD.HHMMSS.sss
            if (fileTime < timeLimitBack) // time string format allows direct comparison
                fs::remove(file.path());
        }
    }
}

std::string VisualizerData::getCloudFilename(const Cloud& cloud, const std::string& cloudName) const
{
    return sFolder + sFilePrefix + cloud.mTimestamp + "." + mName +  "." + std::to_string(cloud.mViewport) + "-view." + cloudName + ".pcd";
}

///////////////////////////////////////////////////////////////////////////////////
// CLOUD

int Cloud::getNbPoints() const
{
    if (getNbFeatures() <= 0)
        return 0;

    return static_cast<int>(mFeatures.begin()->second.size());
}

Cloud& Cloud::setViewport(ViewportIdx viewport)
{
    // Continue using already set viewport (do nothing) if -1.
    if (viewport > 0)
        mViewport = viewport;

    return *this;
}

Cloud& Cloud::addFeature(const FeatureData& data, const FeatureName& name, ViewportIdx viewport)
{
    // assert size if > 0

    if (hasFeature(name))
        getFeatureData(name) = data; // overwrite
    else
        mFeatures.emplace_back(name, data);

    setViewport(viewport);
    return *this;
}

template<>
Cloud& Cloud::addCloud(const pcl::PointCloud<pcl::PointXYZ>& data, ViewportIdx viewport)
{
    using P = pcl::PointXYZ;
    addFeature(data, "x", [](const P& p) { return p.x; }, viewport);
    addFeature(data, "y", [](const P& p) { return p.y; }, viewport);
    addFeature(data, "z", [](const P& p) { return p.z; }, viewport);
    addSpace("x", "y", "z");
    addCloudCommon(viewport);
    return *this;
}

template<>
Cloud& Cloud::addCloud(const pcl::PointCloud<pcl::Normal>& data, ViewportIdx viewport)
{
    using P = pcl::Normal;
    addFeature(data, "normal_x", [](const P& p) { return p.normal_x; }, viewport);
    addFeature(data, "normal_y", [](const P& p) { return p.normal_y; }, viewport);
    addFeature(data, "normal_z", [](const P& p) { return p.normal_z; }, viewport);
    addFeature(data, "curvature", [](const P& p) { return p.curvature; }, viewport);
    addSpace("normal_x", "normal_y", "normal_z");
    addCloudCommon(viewport);
    return *this;
}

template<>
Cloud& Cloud::addCloud(const pcl::PointCloud<pcl::PointNormal>& data, ViewportIdx viewport)
{
    using P = pcl::PointNormal;
    addFeature(data, "x", [](const P& p) { return p.x; }, viewport);
    addFeature(data, "y", [](const P& p) { return p.y; }, viewport);
    addFeature(data, "z", [](const P& p) { return p.z; }, viewport);
    addFeature(data, "normal_x", [](const P& p) { return p.normal_x; }, viewport);
    addFeature(data, "normal_y", [](const P& p) { return p.normal_y; }, viewport);
    addFeature(data, "normal_z", [](const P& p) { return p.normal_z; }, viewport);
    addFeature(data, "curvature", [](const P& p) { return p.curvature; }, viewport);
    addSpace("x", "y", "z");
    addSpace("normal_x", "normal_y", "normal_z");
    addCloudCommon(viewport);
    return *this;
}

template<>
Cloud& Cloud::addCloud(const pcl::PointCloud<pcl::PrincipalCurvatures>& data, ViewportIdx viewport)
{
    using P = pcl::PrincipalCurvatures;
    addFeature(data, "principal_curvature_x", [](const P& p) { return p.principal_curvature_x; }, viewport);
    addFeature(data, "principal_curvature_y", [](const P& p) { return p.principal_curvature_y; }, viewport);
    addFeature(data, "principal_curvature_z", [](const P& p) { return p.principal_curvature_z; }, viewport);
    addFeature(data, "pc1", [](const P& p) { return p.pc1; }, viewport);
    addFeature(data, "pc2", [](const P& p) { return p.pc2; }, viewport);
    addSpace("principal_curvature_x", "principal_curvature_y", "principal_curvature_z");
    // TODO would be nice to support kind of 2d mode, to add pc1/pc2
    addCloudCommon(viewport);
    return *this;
}

void Cloud::addCloudCommon(ViewportIdx viewport)
{
    setViewport(viewport);
    createTimestamp();
}

void Cloud::createTimestamp()
{
    mTimestamp = VisualizerData::createTimestampString();
}

std::string VisualizerData::createTimestampString(int hrsBack)
{
    const auto now = std::chrono::system_clock::now() - std::chrono::hours(hrsBack);
    const auto nowAsTimeT = std::chrono::system_clock::to_time_t(now);
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::stringstream nowSs;
    nowSs << std::put_time(std::localtime(&nowAsTimeT), "%Y%m%d.%H%M%S.") << std::setfill('0') << std::setw(3) << nowMs.count();
    return nowSs.str();
}

Cloud& Cloud::addLabelsFeature(const std::vector< std::vector<int> >& componentsIndixes, const FeatureName& name, ViewportIdx viewport)
{
    const int nbPoints = getNbPoints();

    if (nbPoints <= 0)
        logError("[addLabelsFeature] no points in the specified cloud, addLabelsFeature should be called after at least one call to addCloud.");
    else
    {
        FeatureData labels(nbPoints, -1);

        int label = 0;
        for (const auto& componentIndices : componentsIndixes)
        {
            for (const int i : componentIndices)
            {
                if (i >= 0 && i < nbPoints)
                    labels[i] = label;
                else
                    logError("[addLabelsFeature] indices are out of bounds.");
            }

            ++label;
        }

        addFeature(labels, name, viewport);
    }

    return *this;
}

Cloud& Cloud::addSpace(const FeatureName& a, const FeatureName& b, const FeatureName& c)
{
    if      (!hasFeature(a)) { logError("[addSpace] following feature does not exit: " + a); return *this; }
    else if (!hasFeature(b)) { logError("[addSpace] following feature does not exit: " + b); return *this; }
    else if (!hasFeature(c)) { logError("[addSpace] following feature does not exit: " + c); return *this; }
    else mSpaces.emplace_back(*getFeature(a), *getFeature(b), *getFeature(c));
    return *this;
}

FeatureIt Cloud::getFeature(const FeatureName& name)
{
    return std::find_if(mFeatures.begin(), mFeatures.end(),
        [&name](const Feature& f) {return f.first == name; });
}

FeatureConstIt Cloud::getFeature(const FeatureName& name) const
{
    return std::find_if(mFeatures.cbegin(), mFeatures.cend(),
        [&name](const Feature& f) {return f.first == name; });
}

FeatureData& Cloud::getFeatureData(const FeatureName& name)
{
    if (!hasFeature(name)) logError("Cannot get feature data vector if the feature does not exist.");
    return getFeature(name)->second;
}

const FeatureData& Cloud::getFeatureData(const FeatureName& name) const
{
    if (!hasFeature(name)) logError("Cannot get feature data vector if the feature does not exist.");
    return getFeature(name)->second;
}

bool Cloud::hasFeature(const FeatureName& name) const
{
    return getFeature(name) != mFeatures.end();
}

bool Cloud::hasRgb() const
{
    return mFeatures.end() != std::find_if(
        mFeatures.begin(), mFeatures.end(), [](const Feature& f) { return f.first == "rgb"; });
}

void Cloud::render() const
{
    if (mVisualizerPtr) mVisualizerPtr->render();
}

void Cloud::save(const std::string& filename) const
{
    std::ofstream f;
    f.open(filename);

    // header
    f << "# .PCD v.7 - Point Cloud Data file format" << std::endl;
    f << "VERSION .7" << std::endl;

    f << "FIELDS";
    for (const auto& feature : mFeatures)
        f << " " << feature.first;
    f << std::endl;

    f << "SIZE";
    for (int i = 0; i < getNbFeatures(); ++i)
        f << " 4";
    f << std::endl;

    f << "TYPE";
    for (int i = 0; i < getNbFeatures(); ++i)
        f << (mFeatures[i].first == "rgb" ? " U" : " F");
    f << std::endl;

    f << "COUNT";
    for (int i = 0; i < getNbFeatures(); ++i)
        f << " 1";
    f << std::endl;

    f << "WIDTH " << getNbPoints() << std::endl;
    f << "HEIGHT 1" << std::endl;
    f << "VIEWPOINT 0 0 0 1 0 0 0" << std::endl;
    f << "POINTS " << getNbPoints() << std::endl;
    f << "DATA ascii" << std::endl;

    for (int i = 0; i < getNbPoints(); ++i)
    {
        for (const auto& feature : mFeatures)
        {
            // TODO should make sure that all features have same amount of points
            if (feature.first == "rgb")
                f << static_cast<uint32_t>(feature.second[i]) << " ";
            else
                f << feature.second[i] << " ";
        }
        f << std::endl;
    }

    f.close();
}

Space::Space(const Feature& a, const Feature& b, const Feature& c) : 
    u1(a.first), u2(b.first), u3(c.first),
    mSearchTree(flann::KDTreeSingleIndexParams()) // optimized for 3D, gives exact result
{
    const FeatureData& va = a.second;
    const FeatureData& vb = b.second;
    const FeatureData& vc = c.second;

    const int N = va.size();
    if (va.size() != N || vb.size() != N || vc.size() != N)
        logError("All features must have the same size. Will crash.");

    if (N > 0)
    {
        FeatureData data;
        data.reserve(N * 3);
        for (int i = 0; i < N; ++i)
        {
            data.push_back(va[i]);
            data.push_back(vb[i]);
            data.push_back(vc[i]);
        }

        mSearchTree.buildIndex(flann::Matrix<float>(data.data(), N, 3));
    }
    else
        mSearchTree.buildIndex(); // initialize it with nothing to avoid crash if null
}

int Space::findPickedPointIndex(float a, float b, float c) const
{
    const int nbQueries = 1;
    const int nbDims = 3;

    std::vector<float> queryData({ a, b, c });
    std::vector<int> indicesData(nbQueries, 0);
    std::vector<float> distsData(nbQueries, 0);

    flann::Matrix<float> query(queryData.data(), nbQueries, nbDims);
    flann::Matrix<int> indices(indicesData.data(), nbQueries, 1);
    flann::Matrix<float> dists(distsData.data(), nbQueries, 1);

    mSearchTree.knnSearch(query, indices, dists, nbQueries, flann::SearchParams());

    // Must be a perfect pick, to avoid confusion between clouds in same viewport.
    const float eps = 1e-10;
    return (*dists[0] < eps) ? *indices[0] : -1;
}