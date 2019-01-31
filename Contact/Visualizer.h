#pragma once

#include <stdlib.h>

#include <map>
#include <string>
#include <vector>

#include <pcl/pcl_base.h>
#include <pcl/point_types.h>
#include <pcl/visualization/pcl_visualizer.h>

#define VISUALIZER_CALL(x) x

class PclVisualizer : public pcl::visualization::PCLVisualizer
{
public:
    PclVisualizer(const std::string& name) : pcl::visualization::PCLVisualizer(name) {}

    bool addPointCloudColor(
        const pcl::PCLPointCloud2::Ptr &cloud,
        const ColorHandlerConstPtr &color,
        const GeometryHandlerConstPtr &defaultGeo,
        const std::string &id, int viewport);

};

class Visualizer
{
public:
    Visualizer(const std::string& name, int nbRows = 1, int nbCols = 1);

    using CloudName = std::string;
    using FeatureName = std::string;
    using FeatureData = std::vector<float>;
    using ViewportIdx = int;

    static const std::string sFilePrefix;

    struct ColorRGB 
    {
        // Note: PCL supports RGBA, but it does not seem to be fully supported everywhere (e.g. the PCL viewer).
        ColorRGB(float ri, float gi, float bi) : r(ri), g(gi), b(bi) {}
        float r, g, b;
    };

    struct Space
    {
        Space(const FeatureName& a, const FeatureName& b, const FeatureName& c) : u1(a), u2(b), u3(c) {}
        FeatureName u1, u2, u3;
    };

    class Cloud
    {
    public:
        Cloud() = default;

        template<typename T>
        Cloud& add(const pcl::PointCloud<T>& data, ViewportIdx viewport = -1);
        template<typename T, typename F>
        Cloud& addFeature(const T& data, const FeatureName& featName, F func, ViewportIdx viewport = -1);
        Cloud& addFeature(const FeatureData& data, const FeatureName& name, ViewportIdx viewport = -1);
        Cloud& addSpace(const FeatureName& a, const FeatureName& b, const FeatureName& c);

        Cloud& setViewport(ViewportIdx viewport);
        Cloud& setSize(int size) { mSize = size; return *this; };
        Cloud& setOpacity(double opacity) { mOpacity = opacity; return *this; };
        Cloud& setColor(float r, float g, float b) { mRGB = ColorRGB({r,g,b}); return *this; };

        int getNbPoints() const;
        int getNbFeatures() const { return static_cast<int>(mFeatures.size()); };
        bool hasFeature(const FeatureName& name) const;
        void save(const std::string& filename) const;

        int mViewport{ 0 };
        int mSize{ 1 };
        double mOpacity{ 1.0 };
        ColorRGB mRGB{ -1.0, -1.0, -1.0 };
        std::vector<Space> mSpaces; // using vector instead of [unordered_]map to keep order of insertion
    private:
        std::vector< std::pair<FeatureName, FeatureData> > mFeatures; // using vector instead of [unordered_]map to keep order of insertion
    };

    template<typename T>
    Cloud& add(const pcl::PointCloud<T>& data, const CloudName& name, ViewportIdx viewport = -1);
    template<typename T, typename F>
    Cloud& addFeature(const T& data, const FeatureName& featName, const CloudName& name, F func, ViewportIdx viewport = -1);
    Cloud& addFeature(const FeatureData& data, const FeatureName& featName, const CloudName& cloudName, ViewportIdx viewport = -1);

    int getNbClouds() const { return static_cast<int>(mClouds.size()); };

    void render();

    PclVisualizer& getViewer() { return mViewer; }

private:
    struct State
    {
        int mIdentifiedCloudIdx{ -1 };
    };

    // Interactivity
    void keyboardEventOccurred(const pcl::visualization::KeyboardEvent& event, void*);
    void identifyClouds(bool enabled, bool back);
    void printHelp() const;

    std::string mName;
    PclVisualizer mViewer;
    std::map<CloudName, Cloud> mClouds;
    std::vector<int> mViewportIds;
    State mState;
};

// EXPLICIT INSTANTIATIONS

template<typename T, typename F>
Visualizer::Cloud& Visualizer::addFeature(const T& data, const FeatureName& featName, const CloudName& name, F func, ViewportIdx viewport)
{
    return mClouds[name].addFeature(data, featName, func, viewport);
}

template<typename T, typename F>
Visualizer::Cloud& Visualizer::Cloud::addFeature(const T& data, const FeatureName& featName, F func, ViewportIdx viewport)
{
    FeatureData values(data.size());
    std::transform(std::begin(data), std::end(data), std::begin(values), func);
    return addFeature(values, featName, viewport);
}

template<typename T>
Visualizer::Cloud& Visualizer::add(const pcl::PointCloud<T>& data, const CloudName& name, ViewportIdx viewport)
{
    return mClouds[name].add(data, viewport);
}
