/*
 * phomo Photomosaic Creator
 * Copyright (C) 2009  Peter Goetz
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include <iostream>
#include <map>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <list>
#include <math.h>
#include <sstream>
#include <algorithm>
#include <string>

#include <boost/gil/image.hpp>
#include <boost/gil/typedefs.hpp>

#include <boost/gil/extension/io/jpeg_dynamic_io.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/program_options.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/timer.hpp>
#include <boost/foreach.hpp>

#include <exiv2/image.hpp>

namespace gil = boost::gil;
namespace lambda = boost::lambda;
namespace program_options = boost::program_options;
namespace filesystem = boost::filesystem;


using std::string;
using std::cout;
using std::cerr;
using std::map;
using std::list;
using std::endl;
using std::ofstream;
using std::ifstream;
//using filesystem::recursive_directory_iterator;
using std::vector;
using std::ptrdiff_t;
using boost::algorithm::iends_with;
using boost::lexical_cast;
using boost::algorithm::split;
using boost::algorithm::is_any_of;


const int NUMBER_OF_CHANNELS = 3;

const int RED_CHANNEL_INDEX = 0;
const int GREEN_CHANNEL_INDEX = 1;
const int BLUE_CHANNEL_INDEX = 2;

enum Orientation { NOT_ROTATED = 1, ROTATED_180 = 3, ROTATED_90CCW = 6, ROTATED_90CW = 8 };

program_options::options_description visible_options_description()
{
    program_options::options_description options_description;

    options_description.add_options()
        ("help,h", "Prints this help.")
        ("version,v", "Prints version information.");
    program_options::options_description shared_options("Options shared between build-database and render");
    shared_options.add_options()
        ("database-filename", program_options::value<string>(), "The filename for the photos database.");
    program_options::options_description build_database_options("Options allowed for build-database");
    build_database_options.add_options()
        ("input-type", program_options::value<string>(), "Specifies the input type. Allowed values: directory | file.")
        ("photos-dir", program_options::value<string>(), "Top-directory which will be recursively traversed to build mosaic stones database.")
        ("photos-file", program_options::value<string>(), "File that contains a list of image file paths to be used as mosaic stones. A \"-\" uses standard input instead of a file.")
        ("aspect-ratio", program_options::value<string>()->default_value("1"), "Aspect ratio which should be used for the mosaic stones. Either WidthxHeight or a real number.")
        ("raster-resolution", program_options::value<int>()->default_value(3), "Resolution of the rasterization the algorithm should internally use.");
    program_options::options_description render_options("Options allowed for render");
    render_options.add_options()
        ("picture-path", program_options::value<string>(), "Path of the input pictures that is to be mosaicized.")
        ("output-width", program_options::value<int>()->default_value(1024), "Pixel resolution of the resulting photo mosaic.")
        ("x-resolution-in-stones", program_options::value<int>()->default_value(10), "Resolution of the resulting photo mosaic measured in mosaic stones.")
        ("min-distance", program_options::value<int>()->default_value(10), "The minimum distance in which identical stones are allowed to appear.")
        ("number-of-threads", program_options::value<int>()->default_value(4), "Fine tune control over number of threads to use.")
        ("print-time-left", "Print time left to complete instead of progress in percentage.")
        ("output-filename", program_options::value<string>(), "Image file path for the resulting photo mosaic.");
    options_description.add(shared_options);
    options_description.add(build_database_options);
    options_description.add(render_options);
    return options_description;
}

program_options::options_description full_options_description()
{
    program_options::options_description action;

    action.add_options()
        ("action", program_options::value<string>(), "Allowed values: build-database | render.\n"
                                                     "build-database will build up a database of mosaic stones to use.\n"
                                                     "render will use an existing mosaic stones database to render a picture.");
    action.add(visible_options_description());
    return action;

}

program_options::variables_map parse_command_line(int argc, char** argv)
{
    program_options::options_description options_description = full_options_description();

    program_options::positional_options_description pos_options_desc;
    pos_options_desc.add("action", 1);

    program_options::variables_map result;
    program_options::store(program_options::command_line_parser(argc, argv).options(options_description).positional(
            pos_options_desc).run(), result);
    program_options::notify(result);
    return result;
}


unsigned char get_average_value(const gil::gray8_step_view_t& source_view)
{
    long sum = 0;
    gil::for_each_pixel(source_view, sum += lambda::_1);
    return sum / source_view.size();
}

template<int channel_index, class View>
int average_raster_value(const View& source_view, int col, int row, int raster_width, int raster_height)
{
    gil::gray8_step_view_t sub_view(gil::subimage_view(gil::kth_channel_view<channel_index>(source_view), col * raster_width, row
            * raster_height, raster_width, raster_height));
    return get_average_value(sub_view);
}

template<class View>
void raster_values_from_view(const View& source_view, int col_count, int row_count, vector<int>& raster_values)
{
    assert(raster_values.size() == (size_t)(col_count*row_count*NUMBER_OF_CHANNELS));
    typename View::x_coord_t raster_width = source_view.width() / col_count;
    typename View::y_coord_t raster_height = source_view.height() / row_count;

    for (int y = 0; y < row_count; ++y)
    {
        for (int x = 0; x < col_count; ++x)
        {
            raster_values[x + y*col_count + RED_CHANNEL_INDEX*col_count*row_count] =
                average_raster_value<RED_CHANNEL_INDEX>(source_view, x,y, raster_width, raster_height);
            raster_values[x + y*col_count + GREEN_CHANNEL_INDEX*col_count*row_count] =
                average_raster_value<GREEN_CHANNEL_INDEX>(source_view, x,y, raster_width, raster_height);
            raster_values[x + y*col_count + BLUE_CHANNEL_INDEX*col_count*row_count] =
                average_raster_value<BLUE_CHANNEL_INDEX>(source_view, x,y, raster_width, raster_height);
        }
    }
}

Orientation orientation_from_image_path(const string& path)
{
    Exiv2::Image::AutoPtr exif_image = Exiv2::ImageFactory::open(path);
    exif_image->readMetadata();
    Exiv2::ExifData &exifData = exif_image->exifData();
    if(exifData.empty())
    {
        return NOT_ROTATED;
    }
    switch (exifData["Exif.Image.Orientation"].toLong())
    {
    case 1:
        return NOT_ROTATED;
    case 3:
        return ROTATED_180;
    case 6:
        return ROTATED_90CCW;
    case 8:
        return ROTATED_90CW;
    default:
        return NOT_ROTATED;
    }

}

typedef gil::point2<std::ptrdiff_t> Dimensions;
typedef gil::point2<std::ptrdiff_t> Position;

void swap(gil::point2<std::ptrdiff_t>& dimensions)
{
    std::ptrdiff_t help = dimensions.x;
    dimensions.x = dimensions.y;
    dimensions.y = help;
}

gil::point2<std::ptrdiff_t> aspect_ratio_cropped_dimensions(const string& current_path, double aspect_ratio, Orientation orientation)
{
    gil::point2<std::ptrdiff_t> dimensions = gil::jpeg_read_dimensions(current_path);
    if (orientation == ROTATED_90CCW or orientation == ROTATED_90CW)
    {
        swap(dimensions);
    }
    if(dimensions.x/aspect_ratio > dimensions.y)
    {
        dimensions.x = dimensions.y*aspect_ratio;
    }
    else
    {
        dimensions.y = dimensions.x/aspect_ratio;
    }
    return dimensions;
}

class Timer
{
    boost::timer timer_;
public:
    void restart() {
#ifdef PHOMO_TIMER
        timer_.restart();
#endif
    }
    void print_elapsed_with_label(const string& label)
    {
#ifdef PHOMO_TIMER
        cout << label << ": " << timer_.elapsed() << "seconds" << endl;
#endif
    }
} timer;


class MosaicStone
{
    string image_file_path_;
    vector<int> raster_values_;
    int id_;
public:
    MosaicStone() {}
    MosaicStone(const string &image_file_path, const vector<int>& raster_values, int id) :
        image_file_path_(image_file_path),
        raster_values_(raster_values),
        id_(id) { }

    MosaicStone(const string &image_file_path, int col_count, int row_count, double aspect_ratio) :
        image_file_path_(image_file_path),
        raster_values_(col_count*row_count*NUMBER_OF_CHANNELS, 0)
    {
        timer.restart();
        Orientation orientation = orientation_from_image_path(image_file_path);
        gil::point2<std::ptrdiff_t> dimensions = aspect_ratio_cropped_dimensions(image_file_path, aspect_ratio, orientation);

        gil::rgb8_image_t source_image;
        gil::jpeg_read_and_convert_image(image_file_path, source_image);
        gil::rgb8_view_t source_view = view(source_image);
        timer.print_elapsed_with_label("Load image");
        timer.restart();
        switch(orientation)
        {
        case NOT_ROTATED:
            raster_values_from_view(/*gil::subsampled_view(*/gil::subimage_view(source_view, 0, 0, dimensions.x, dimensions.y)/*, 5, 5)*/, col_count, row_count, raster_values_);
            break;
        case ROTATED_180:
            raster_values_from_view(/*gil::subsampled_view(*/gil::subimage_view(rotated180_view(source_view), 0, 0, dimensions.x, dimensions.y)/*, 5, 5)*/, col_count, row_count, raster_values_);
            break;
        case ROTATED_90CCW:
            raster_values_from_view(/*gil::subsampled_view(*/gil::subimage_view(rotated90cw_view(source_view), 0, 0, dimensions.x, dimensions.y)/*, 5, 5)*/, col_count, row_count, raster_values_);
            break;
        case ROTATED_90CW:
            raster_values_from_view(/*gil::subsampled_view(*/gil::subimage_view(rotated90ccw_view(source_view), 0, 0, dimensions.x, dimensions.y)/*, 5, 5)*/, col_count, row_count, raster_values_);
            break;
        }
        timer.print_elapsed_with_label("raster_value_from_view");
    }

    void set_id(int id) { id_=id; }
    int id() const { return id_; }

    const string& image_file_path() const { return image_file_path_; }

    int operator[](int index) const { return raster_values_[index]; }
//    vector<int>& operator[](int index) { return raster_values_[index]; }

    int calc_deviation(const vector<int>& b, int best_deviation) const
    {
        assert(raster_values_.size() == b.size());
        int deviation = 0;
        for(size_t i=0; i<raster_values_.size();++i)
        {
            deviation += (raster_values_[i]-b[i])*(raster_values_[i]-b[i]);

            if(deviation>=best_deviation and best_deviation!=-1)
            {
                return -1;
            }
        }
        return deviation;
    }
};

typedef boost::shared_ptr<MosaicStone> MosaicStonePtr;

class MosaicsDatabase
{
public:
    MosaicsDatabase(const string& db_filename) /*:
        file_(db_filename.c_str(), std::ios_base::in)*/
    {
        file_.open(db_filename.c_str(), std::ios_base::in);
        file_ >> aspect_ratio_;
        file_ >> raster_resolution_;
        string line;
        std::getline(file_, line);
        int line_number = 1;
        while(!file_.eof())
        {
            std::getline(file_, line);
            if(line == "")
            {
                continue;
            }
            cout << line << endl;
            vector<string> parts;
            split(parts, line, is_any_of("|"));

            string image_file_path = parts[0];
            vector<int> values(raster_resolution_*raster_resolution_*NUMBER_OF_CHANNELS);
            for(int i=0;i<raster_resolution_*raster_resolution_*NUMBER_OF_CHANNELS;++i)
            {
                values[i] = lexical_cast<int>(parts[i+1]);
            }
            stones_.push_back(MosaicStonePtr(new MosaicStone(image_file_path, values, line_number)));
            line_number++;
        }
    }

    MosaicsDatabase(const string& db_filename, double aspect_ratio, int raster_resolution) :
        file_(db_filename.c_str(), std::ios_base::out | std::ios_base::trunc), raster_resolution_(raster_resolution), aspect_ratio_(aspect_ratio),
        cached_raster_value_count_(raster_resolution*raster_resolution*NUMBER_OF_CHANNELS)
    {
        file_ << aspect_ratio << endl;
        file_ << raster_resolution << endl;
    }

    double aspect_ratio() const { return aspect_ratio_; }

    double raster_resolution() const { return raster_resolution_; }

    void add_mosaic_stone(MosaicStonePtr mosaic_stone)
    {
        boost::mutex::scoped_lock lock(io_mutex);
        file_ << mosaic_stone->image_file_path();
        for(int i=0; i < cached_raster_value_count_; ++i)
        {
            file_ << "|" << (*mosaic_stone)[i];
        }
        file_ << endl;
        stones_.push_back(mosaic_stone);
    }

    const list<MosaicStonePtr>& stones() const { return stones_; }

private:
    std::fstream file_;
    list<MosaicStonePtr> stones_;
    int raster_resolution_;
    double aspect_ratio_;
    int cached_raster_value_count_;
    boost::mutex io_mutex;
};


void add_mosaic_stone_to_database(MosaicsDatabase* mosaics_database, int i, const string& current_path, int col_count, int row_count, double aspect_ratio)
{
    std::stringstream output;
    output << i << "(thread-id: " <<  boost::this_thread::get_id() << ") "<< " " << current_path << " ... ";
    try
    {
        mosaics_database->add_mosaic_stone(MosaicStonePtr(new MosaicStone(current_path, col_count, row_count, aspect_ratio)));
        output << "Added" << std::endl;
        cout << output.str();
    }
    catch(std::exception& error)
    {
        output << "Error: " << error.what() << " ==> skipping" << std::endl;
        cerr << output.str();
    }
    catch(...)
    {
        output << "Unknown error ==> skipping" << std::endl;
        cerr << output.str();
    }
}


class ImageFilePathIterator
{
public:
    virtual ~ImageFilePathIterator() = 0;
    virtual string get_next() = 0;
    virtual int counter() const = 0;
};

typedef boost::shared_ptr<ImageFilePathIterator> ImageFilePathIteratorPtr;

ImageFilePathIterator::~ImageFilePathIterator() {}


class StopIteration {};

class RecursiveDirectoryImageFilePathIterator : public ImageFilePathIterator
{
    filesystem::recursive_directory_iterator dir_it_;
    filesystem::recursive_directory_iterator end_;
    mutable boost::mutex mutex;
    int i;
public:
    RecursiveDirectoryImageFilePathIterator(const string& photos_dir_path) :
        dir_it_(photos_dir_path), i(0) {}

    virtual string get_next() {
        boost::mutex::scoped_lock lock(mutex);
        if (dir_it_ == end_)
        {
            throw StopIteration();
        }
        string path = dir_it_->path().string();
        ++dir_it_;
        ++i;
        return path;
    }

    virtual int counter() const {
        boost::mutex::scoped_lock lock(mutex);
        return i;
    }
};

class InputStreamImageFilePathIterator : public ImageFilePathIterator
{
    int i;
    boost::shared_ptr<std::istream> inputStream_;
    bool done_;
    mutable boost::mutex mutex_;
public:
    InputStreamImageFilePathIterator(boost::shared_ptr<std::istream> inputStream) : i(0), inputStream_(inputStream), done_(false) {}
    virtual string get_next() {
        string result;
        boost::mutex::scoped_lock lock(mutex_);
        if(done_)
        {
            throw StopIteration();
        }
        std::getline(*inputStream_, result);
        if (result.empty())
        {
            done_ = true;
            throw StopIteration();
        }
        i++;
        return result;
    }

    virtual int counter() const {
        boost::mutex::scoped_lock lock(mutex_);
        return i;
    }
};

void add_stones_to_database(MosaicsDatabase* mosaics_database, ImageFilePathIteratorPtr image_file_it, double aspect_ratio, int raster_resolution)
{
    while(true)
    {
        try
        {
            string current_path = image_file_it->get_next();
            int i = image_file_it->counter();
            if (iends_with(current_path, ".JPG"))
            {
                add_mosaic_stone_to_database(mosaics_database, i, current_path, raster_resolution, raster_resolution, aspect_ratio);
            }
            else
            {
                cout << i << " " << current_path << " ... "<< "No JPG ==> NOT Added" << std::endl;
            }
        }
        catch (StopIteration&) {
            break;
        }
    }
}

typedef boost::shared_ptr<boost::thread> ThreadPtr;
typedef list<ThreadPtr> ThreadList;

void build_database(ImageFilePathIteratorPtr image_file_it, const string& output_filename, double aspect_ratio, int raster_resolution, int number_of_threads)
{
    MosaicsDatabase mosaics_database(output_filename, aspect_ratio, raster_resolution);

    ThreadList thread_list;
    for(int i=0;i<number_of_threads;++i)
    {
        thread_list.push_back(ThreadPtr(new boost::thread(add_stones_to_database, &mosaics_database, image_file_it, aspect_ratio, raster_resolution)));
    }
    BOOST_FOREACH(ThreadPtr thread, thread_list)
    {
        thread->join();
    }
}


MosaicStonePtr find_closest_match(const list<MosaicStonePtr>& stones, const vector<int>& rastered_piece, const list<MosaicStonePtr>& excluded)
{
    int best_deviation = -1;
    list<MosaicStonePtr>::const_iterator best_stone = stones.end();
    for(list<MosaicStonePtr>::const_iterator stone = stones.begin(); stone != stones.end(); ++stone)
    {
        if (std::find(excluded.begin(), excluded.end(), *stone) == excluded.end())
        {
            int deviation = (*stone)->calc_deviation(rastered_piece, best_deviation);
            if (deviation != -1)
            {
                best_deviation = deviation;
                best_stone = stone;
            }
        }
    }
    if(best_stone == stones.end())
    {
        throw std::runtime_error("Not enough stones for current parameters. Try reducing min-distance.");
    }
    return *best_stone;
}


class JPG
{
public:
    JPG(const Dimensions& dimensions) :
        image_(dimensions)
    {
       view_ = view(image_);
    }

    void write(const string& filename)
    {
        gil::jpeg_write_view(filename, view_, 85);
    }

    void set_mosaic_stone(int x, int y, int stone_width, int stone_height, const string& current_path)
    {
        try
        {
            double aspect_ratio = (double)stone_width/(double)stone_height;
            Orientation orientation = orientation_from_image_path(current_path);
            gil::point2<std::ptrdiff_t> dimensions = aspect_ratio_cropped_dimensions(current_path, aspect_ratio, orientation);
            gil::rgb8_image_t mosaic_stone_img_big;
            gil::jpeg_read_image(current_path, mosaic_stone_img_big);

            gil::rgb8_image_t mosaic_stone_img_small(stone_width, stone_height);

            Position o;

            switch(orientation)
            {
            case NOT_ROTATED:
                gil::resize_view(gil::subimage_view(const_view(mosaic_stone_img_big), o, dimensions),
                        view(mosaic_stone_img_small), gil::bilinear_sampler());
                break;
            case ROTATED_180:
                gil::resize_view(gil::subimage_view(rotated180_view(const_view(mosaic_stone_img_big)), o, dimensions),
                        view(mosaic_stone_img_small), gil::bilinear_sampler());
                break;
            case ROTATED_90CCW:
                gil::resize_view(gil::subimage_view(rotated90cw_view(const_view(mosaic_stone_img_big)), o, dimensions),
                        view(mosaic_stone_img_small), gil::bilinear_sampler());
                break;
            case ROTATED_90CW:
                gil::resize_view(gil::subimage_view(rotated90ccw_view(const_view(mosaic_stone_img_big)), o, dimensions),
                        view(mosaic_stone_img_small), gil::bilinear_sampler());
                break;
            }

            {
                boost::mutex::scoped_lock lock(mutex);
                gil::copy_pixels(const_view(mosaic_stone_img_small), subimage_view(view_, x * stone_width, y * stone_height, stone_width, stone_height));
            }
        }
        catch(std::exception& error)
        {
            cerr << "Error setting mosaic stone: "<< error.what() << endl;
        }
    }

private:
    string filename_;
    gil::rgb8_image_t image_;
    gil::rgb8_view_t view_;
    boost::mutex mutex;
};

class OutputMatrix
{
    vector<vector<MosaicStonePtr> > matrix_;
    mutable boost::mutex mutex;

public:
    OutputMatrix(const Dimensions& dimensions) : matrix_(dimensions.x, vector<MosaicStonePtr>(dimensions.y))
    {}

    OutputMatrix(const OutputMatrix& other)
    {
        matrix_ = other.matrix_;
    }

    MosaicStonePtr& operator()(int x, int y)
    {
        boost::mutex::scoped_lock lock(mutex);
        return matrix_[x][y];
    }

    const MosaicStonePtr& operator() (int x, int y) const
    {
        boost::mutex::scoped_lock lock(mutex);
        return matrix_[x][y];
    }

    int xres() const {
        boost::mutex::scoped_lock lock(mutex);
        return matrix_.size();
    }

    int yres() const {
        boost::mutex::scoped_lock lock(mutex);
        return matrix_[0].size();
    }
};

int start_from_coord_and_min_dinstance(int coord, int min_distance)
{
    int result = coord - min_distance;
    if (result<0)
    {
        result = 0;
    }
    return result;
}

int end_from_coord_and_min_distance(int coord, int min_distance, int resolution)
{
    int end = coord+min_distance;
    if (end > resolution-1)
    {
        end=resolution-1;
    }
    return end;
}

list<MosaicStonePtr> create_distance_caused_excludes(int x, int y, const OutputMatrix& output, int min_distance)
{
    int startx = start_from_coord_and_min_dinstance(x, min_distance);
    int starty = start_from_coord_and_min_dinstance(y, min_distance);
    int endx = end_from_coord_and_min_distance(x, min_distance, output.xres());
    int endy = end_from_coord_and_min_distance(y, min_distance, output.yres());
    list<MosaicStonePtr> excludes;
    for(int i=startx; i<=endx; ++i)
    {
        for(int j=starty; j<=endy; ++j)
        {
            if((i!=x or j!=y) and output(i,j))
            {
                excludes.push_back(output(i,j));
            }
        }
    }
    return excludes;
}

struct Configuration
{
    int x_res_in_stones;
    int min_distance;
};

struct Size
{
    int width;
    int height;
};

struct Limits
{
    int min;
    int max;
};

boost::mutex mosaic_stone_set_mutex;

class Progress
{
    int hundred_percent_equivalent_;
    int done_so_far_;
    mutable boost::mutex mutex_;

    boost::timer timer_;
    bool print_time_left_;
public:
    Progress(int hundred_percent_equivalent, bool print_time_left = false)
        : hundred_percent_equivalent_(hundred_percent_equivalent),
          done_so_far_(0),
          print_time_left_(print_time_left)
          {}

    double status() const
    {
        return (double)done_so_far_/hundred_percent_equivalent_;
    }

    void inc_and_print()
    {
        if(print_time_left_)
        {
            inc_and_print_minutes_left();
        }
        else
        {
            inc_and_print_status();
        }
    }

    void inc_and_print_status()
    {
        boost::mutex::scoped_lock lock(mutex_);
        ++done_so_far_;
        cout << status() * 100 << "%" << endl;
    }

    void inc_and_print_minutes_left()
    {
        boost::mutex::scoped_lock lock(mutex_);
        ++done_so_far_;
        cout << minutes_left() << " minutes left" << endl;
    }

    double minutes_left() const
    {
        return (1.0-status())*timer_.elapsed()/status() / 60;
    }
};

struct RenderSettings
{
    ptrdiff_t min_distance;
    Dimensions output_dimensions;
    Dimensions resolution_in_stones;
    RenderSettings(const Dimensions& input_dimensions, ptrdiff_t output_width, ptrdiff_t x_resolution_in_stones, ptrdiff_t min_distance_, double aspect_ratio) :
        min_distance(min_distance_)
    {
        resolution_in_stones.x = x_resolution_in_stones;
        int source_stone_width = input_dimensions.x / resolution_in_stones.x;
        int source_stone_height = (double)source_stone_width / aspect_ratio;
        resolution_in_stones.y = input_dimensions.y / source_stone_height;

        output_dimensions.x = output_width;
        int output_stone_width = output_dimensions.x/resolution_in_stones.x;
        int output_stone_height = (double)output_stone_width / aspect_ratio;
        output_dimensions.y = output_stone_height * resolution_in_stones.y;
    }
};

template<class SourceView>
struct RenderParameters
{
    Limits row_limits;
    int min_distance;
    Size source_stone_size;
    Size output_stone_size;
    SourceView source_view;
    MosaicsDatabase* mosaics_database;
    JPG* output_image;
    OutputMatrix* output;
    Progress* progress;
    vector<Position> *positions;
};

template<class SourceView>
void find_and_set_stones(RenderParameters<SourceView> params)
{
    cout << params.row_limits.min << " " << params.row_limits.max << endl;
    const list<MosaicStonePtr> &stones = params.mosaics_database->stones();
    OutputMatrix& output_matrix = *params.output;

    for(int i=params.row_limits.min; i<=params.row_limits.max; ++i)
    {
        int stone_x = (*(params.positions))[i].x;
        int stone_y = (*(params.positions))[i].y;
        timer.restart();
        SourceView subimage = subimage_view(params.source_view, stone_x*params.source_stone_size.width, stone_y*params.source_stone_size.height, params.source_stone_size.width, params.source_stone_size.height);


        int raster_resolution = params.mosaics_database->raster_resolution();
        vector<int> rastered_piece(raster_resolution*raster_resolution*NUMBER_OF_CHANNELS);
        raster_values_from_view(subimage, raster_resolution, raster_resolution, rastered_piece);
        timer.print_elapsed_with_label("Elapsed time to create rastered piece");

        MosaicStonePtr mosaic_stone;

        {
            boost::mutex::scoped_lock lock(mosaic_stone_set_mutex);

            timer.restart();
            list<MosaicStonePtr> excluded_stones = create_distance_caused_excludes(stone_x, stone_y, output_matrix, params.min_distance);
            timer.print_elapsed_with_label("Elapsed time to find excludes");

            timer.restart();
            mosaic_stone = find_closest_match(stones, rastered_piece, excluded_stones);
            timer.print_elapsed_with_label("Elapsed time to find stone");

            output_matrix(stone_x, stone_y) = mosaic_stone;
        }

        params.output_image->set_mosaic_stone(stone_x, stone_y, params.output_stone_size.width, params.output_stone_size.height, mosaic_stone->image_file_path());
//            progress->inc_and_print_status();
        params.progress->inc_and_print();
    }

}


struct Shuffler
{
    virtual ~Shuffler() = 0;
    virtual void shuffle(vector<Position>& positions) const = 0;
};

Shuffler::~Shuffler() {}

struct RandomShuffler
{
    virtual void shuffle(vector<Position>& positions) const
    {
        std::random_shuffle(positions.begin(), positions.end());
    }
};

inline int calc_source_stone_width(int source_view_width, int xres_in_stones)
{
    return source_view_width / xres_in_stones;
}

inline int calc_source_stone_height(int source_view_width, int xres_in_stones, double aspect_ratio)
{
    return (double)calc_source_stone_width(source_view_width, xres_in_stones) / aspect_ratio;
}

inline int calc_y_res_in_stones(int source_view_width, int source_view_height, int xres_in_stones, double aspect_ratio)
{
    return source_view_height / calc_source_stone_height(source_view_width, xres_in_stones, aspect_ratio);
}

class Renderer
{

public:
    Renderer(MosaicsDatabase& mosaics_database, int number_of_threads) :
        number_of_threads_(number_of_threads), mosaics_database_(mosaics_database) {}

    template<class SourceView>
    OutputMatrix render(const SourceView& source_view, JPG& output_image, const RenderSettings& render_settings, bool print_time_left)
    {
        int source_stone_width = source_view.width() / render_settings.resolution_in_stones.x;
        int source_stone_height = source_stone_width / mosaics_database_.aspect_ratio();
        int output_stone_width = render_settings.output_dimensions.x / render_settings.resolution_in_stones.x;
        int output_stone_height = (double)output_stone_width / mosaics_database_.aspect_ratio();

        OutputMatrix output(render_settings.resolution_in_stones);

        int number_of_stones = render_settings.resolution_in_stones.y * render_settings.resolution_in_stones.x;
        vector<Position> positions(number_of_stones);

        if(static_cast<size_t>(render_settings.min_distance *render_settings.min_distance) > mosaics_database_.stones().size())
        {
            throw std::runtime_error("Not enough stones for current settings. Either use a bigger database or reduce min-distance.");
        }

        for(int i=0;i<render_settings.resolution_in_stones.y;++i)
        {
            for(int j=0;j<render_settings.resolution_in_stones.x;++j)
            {
                Position position; position.x = j; position.y = i;
                positions[i*render_settings.resolution_in_stones.x+j] = position;
            }
        }
        std::random_shuffle(positions.begin(), positions.end());
        if(number_of_threads_ > number_of_stones)
        {
            throw std::runtime_error("Cannot use more threads than mosaic stones.");
        }
        int stones_per_thread = number_of_stones / number_of_threads_;

        Progress progress(number_of_stones, print_time_left);

        ThreadList thread_list;
        RenderParameters<SourceView> render_parameters;
        render_parameters.min_distance = render_settings.min_distance;
        render_parameters.source_stone_size.width = source_stone_width;
        render_parameters.source_stone_size.height = source_stone_height;
        render_parameters.output_stone_size.width = output_stone_width;
        render_parameters.output_stone_size.height = output_stone_height;
        render_parameters.mosaics_database = &mosaics_database_;
        render_parameters.output = &output;
        render_parameters.output_image = &output_image;
        render_parameters.progress = &progress;
        render_parameters.positions = &positions;
        render_parameters.source_view = source_view;
        for(int i=0; i<number_of_threads_-1;++i)
        {
            render_parameters.row_limits.min = i*stones_per_thread;
            render_parameters.row_limits.max = render_parameters.row_limits.min+stones_per_thread-1;
            thread_list.push_back(ThreadPtr(new boost::thread(find_and_set_stones<SourceView>, render_parameters)));
        }
        render_parameters.row_limits.min = (number_of_threads_-1)*stones_per_thread;
        render_parameters.row_limits.max = number_of_stones-1;
        thread_list.push_back(ThreadPtr(new boost::thread(find_and_set_stones<SourceView>, render_parameters)));

        BOOST_FOREACH(ThreadPtr thread, thread_list)
        {
            thread->join();
        }
        return output;
    }
private:
    int number_of_threads_;
    MosaicsDatabase& mosaics_database_;
};

double aspect_ratio_from_input(const string& input)
{
    double aspect_ratio;
    if (input.find('x') == string::npos)
    {
        aspect_ratio = lexical_cast<double>(input);
    }
    else
    {
        vector<string> dimensions_string;
        split(dimensions_string, input, is_any_of("x"));
        aspect_ratio = lexical_cast<double>(dimensions_string[0])/lexical_cast<double>(dimensions_string[1]);
    }
    return aspect_ratio;
}

void non_deleter(std::istream* p) {}

ImageFilePathIteratorPtr createImageFilePathIterator(const program_options::variables_map& input)
{
    if(input["input-type"].as<string>() == "directory")
    {
        return ImageFilePathIteratorPtr(new RecursiveDirectoryImageFilePathIterator(input["photos-dir"].as<string> ()));
    }
    else if (input["input-type"].as<string>() == "file")
    {
        if(input["photos-file"].as<string>() == "-")
        {
            return ImageFilePathIteratorPtr(new InputStreamImageFilePathIterator(boost::shared_ptr<std::istream>(&std::cin, non_deleter)));
        }
        else
        {
            boost::shared_ptr<std::istream> inputStream(new std::ifstream(input["photos-file"].as<string>().c_str()));
            return ImageFilePathIteratorPtr(new InputStreamImageFilePathIterator(inputStream));
        }
    }
    else
    {
        throw std::runtime_error("Invalid input type.");
    }
}

string help()
{
    std::stringstream output;
    output << "USAGE: " << endl
        << "phomo build-database <build-databse-options>" << endl
        << "phomo render <render-options>" << endl
        << "phomo -h | -v\n\n"
     << visible_options_description();
    return output.str();
}

string version()
{
    return "phomo " VERSION;
}

Dimensions swap_dimensions_if(const Dimensions& dimensions, Orientation orientation)
{
    if (orientation == NOT_ROTATED || orientation == ROTATED_180)
    {
        return dimensions;
    }
    else
    {
        return Dimensions(dimensions.y, dimensions.x);
    }
}

int main(int argc, char** argv)
{
    program_options::variables_map input = parse_command_line(argc, argv);

    if(input.count("help"))
    {
        cout << help() << endl;
    }
    else if (input.count("version"))
    {
        cout << version() << endl;
    }
    else if (input.count("action"))
    {
        if (input["action"].as<string> () == "build-database")
        {
            ImageFilePathIteratorPtr it = createImageFilePathIterator(input);
            double aspect_ratio = aspect_ratio_from_input(input["aspect-ratio"].as<string>());
            build_database(it,
                input["database-filename"].as<string> (),
                aspect_ratio, input["raster-resolution"].as<int>(), input["number-of-threads"].as<int>());
        }
        else if (input["action"].as<string> () == "render")
        {
            string source_img_path = input["picture-path"].as<string>();
            Orientation orientation = orientation_from_image_path(source_img_path);

            gil::rgb8_image_t source_image;
            gil::jpeg_read_and_convert_image(source_img_path, source_image);
            gil::rgb8_view_t source_view = view(source_image);

            MosaicsDatabase mosaics_database(input["database-filename"].as<string> ());

            Renderer renderer(mosaics_database, input["number-of-threads"].as<int>());
            RenderSettings renderSettings(
                    swap_dimensions_if(source_view.dimensions(), orientation),
                    input["output-width"].as<int>(),
                    input["x-resolution-in-stones"].as<int>(),
                    input["min-distance"].as<int>(),
                    mosaics_database.aspect_ratio());

            JPG output_image(renderSettings.output_dimensions);

            switch(orientation)
            {
            case NOT_ROTATED:
                renderer.render(source_view,
                    output_image, renderSettings,
                    input.count("print-time-left"));
                break;
            case ROTATED_180:
                renderer.render(gil::rotated180_view(source_view),
                    output_image, renderSettings,
                    input.count("print-time-left"));
                break;
            case ROTATED_90CCW:
                renderer.render(gil::rotated90cw_view(source_view),
                    output_image, renderSettings,
                    input.count("print-time-left"));
                break;
            case ROTATED_90CW:
                renderer.render(gil::rotated90ccw_view(source_view),
                    output_image, renderSettings,
                    input.count("print-time-left"));
                break;
            }
            output_image.write(input["output-filename"].as<string>());
        }
        else
        {
            cerr << "No action was specified." << endl;
            cout << help() << endl;
        }
    }
    else
    {
        cerr << "No action was specified." << endl;
        cout << help() << endl;
    }
    return 0;
}
