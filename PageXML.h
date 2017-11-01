/**
 * Header file for the PageXML class
 *
 * @version $Version: 2017.11.01$
 * @copyright Copyright (c) 2016-present, Mauricio Villegas <mauricio_ville@yahoo.com>
 * @license MIT License
 */

#ifndef __PAGEXML_H__
#define __PAGEXML_H__

#include <vector>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <opencv2/opencv.hpp>

#if defined (__PAGEXML_LIBCONFIG__)
#include <libconfig.h++>
#endif

#if defined (__PAGEXML_LEPT__)
#include <../leptonica/allheaders.h>
#elif defined (__PAGEXML_MAGICK__)
#include <Magick++.h>
#endif

#if defined (__PAGEXML_OGR__)
#include <ogrsf_frmts.h>
#endif

#if defined (__PAGEXML_LEPT__)
typedef Pix * PageImage;
#elif defined (__PAGEXML_MAGICK__)
typedef Magick::Image PageImage;
#elif defined (__PAGEXML_CVIMG__)
typedef cv::Mat PageImage;
#endif

enum PAGEXML_SETTING {
  PAGEXML_SETTING_INDENT = 0,      // "indent"
  PAGEXML_SETTING_PAGENS,          // "pagens"
  PAGEXML_SETTING_GRAYIMG,         // "grayimg"
  PAGEXML_SETTING_EXTENDED_NAMES   // "extended_names"
};

enum PAGEXML_INSERT {
  PAGEXML_INSERT_CHILD = 0,
  PAGEXML_INSERT_NEXTSIB,
  PAGEXML_INSERT_PREVSIB
};

enum PAGEXML_READ_DIRECTION {
  PAGEXML_READ_DIRECTION_LTR = 0,
  PAGEXML_READ_DIRECTION_RTL,
  PAGEXML_READ_DIRECTION_TTB,
  PAGEXML_READ_DIRECTION_BTT
};

struct NamedImage {
  std::string id;
  std::string name;
  float rotation = 0.0;
  int direction = 0;
  int x = 0;
  int y = 0;
  PageImage image;
  xmlNodePtr node = NULL;

  NamedImage() {};
  NamedImage( std::string _id,
              std::string _name,
              float _rotation,
              int _direction,
              int _x,
              int _y,
              PageImage _image,
              xmlNodePtr _node
            ) {
    id = _id;
    name = _name;
    rotation = _rotation;
    direction = _direction;
    x = _x;
    y = _y;
    image = _image;
    node = _node;
  }
};

class PageXML {
  public:
    static const char* settingNames[];
    static char* version();
    static void printVersions( FILE* file = stdout );
    ~PageXML();
    PageXML();
#if defined (__PAGEXML_LIBCONFIG__)
    PageXML( const libconfig::Config& config );
    PageXML( const char* cfgfile );
    void loadConf( const libconfig::Config& config );
#endif
    void printConf( FILE* file = stdout );
    void newXml( const char* creator, const char* image, const int imgW = 0, const int imgH = 0 );
    void loadXml( const char* fname );
    void loadXml( int fnum, bool prevfree = true );
#if defined (__PAGEXML_LEPT__) || defined (__PAGEXML_MAGICK__) || defined (__PAGEXML_CVIMG__)
    void loadImage( const char* fname = NULL, const bool check_size = true );
#endif
    int simplifyIDs();
    void relativizeImageFilename( const char* xml_path );
    bool areIDsUnique();
    std::vector<NamedImage> crop( const char* xpath, cv::Point2f* margin = NULL, bool opaque_coords = true, const char* transp_xpath = NULL );
    static std::vector<cv::Point2f> stringToPoints( const char* spoints );
    static std::vector<cv::Point2f> stringToPoints( std::string spoints );
    static std::string pointsToString( std::vector<cv::Point2f> points, bool rounded = false );
    static std::string pointsToString( std::vector<cv::Point> points );
    static void pointsLimits( std::vector<cv::Point2f>& points, double& xmin, double& xmax, double& ymin, double& ymax );
    static void pointsBBox( std::vector<cv::Point2f>& points, std::vector<cv::Point2f>& bbox );
    static bool isBBox( const std::vector<cv::Point2f>& points );
    int count( const char* xpath, xmlNodePtr basenode );
    int count( std::string xpath, xmlNodePtr basenode );
    std::vector<xmlNodePtr> select( const char* xpath, xmlNodePtr basenode = NULL );
    std::vector<xmlNodePtr> select( std::string xpath, xmlNodePtr basenode = NULL );
    xmlNodePtr selectNth( const char* xpath, unsigned num, xmlNodePtr basenode = NULL );
    xmlNodePtr selectNth( std::string xpath, unsigned num, xmlNodePtr basenode = NULL );
    static bool nodeIs( xmlNodePtr node, const char* name );
    bool getAttr( const xmlNodePtr node,   const char* name,       std::string& value );
    bool getAttr( const char* xpath,       const char* name,       std::string& value );
    bool getAttr( const std::string xpath, const std::string name, std::string& value );
    int setAttr( std::vector<xmlNodePtr> nodes, const char* name,       const char* value );
    int setAttr( xmlNodePtr node,               const char* name,       const char* value );
    int setAttr( const char* xpath,             const char* name,       const char* value );
    int setAttr( const std::string xpath,       const std::string name, const std::string value );
    xmlNodePtr addElem( const char* name,       const char* id,       const xmlNodePtr node,   PAGEXML_INSERT itype = PAGEXML_INSERT_CHILD, bool checkid = false );
    xmlNodePtr addElem( const char* name,       const char* id,       const char* xpath,       PAGEXML_INSERT itype = PAGEXML_INSERT_CHILD, bool checkid = false );
    xmlNodePtr addElem( const std::string name, const std::string id, const std::string xpath, PAGEXML_INSERT itype = PAGEXML_INSERT_CHILD, bool checkid = false );
    void rmElem( const xmlNodePtr& node );
    int rmElems( const std::vector<xmlNodePtr>& nodes );
    int rmElems( const char* xpath,       xmlNodePtr basenode = NULL );
    int rmElems( const std::string xpath, xmlNodePtr basenode = NULL );
    void setRotation( const xmlNodePtr elem, const float rotation );
    void setReadingDirection( const xmlNodePtr elem, PAGEXML_READ_DIRECTION direction );
    float getRotation( const xmlNodePtr elem );
    int getReadingDirection( const xmlNodePtr elem );
    float getXheight( const xmlNodePtr node );
    float getXheight( const char* id );
    std::vector<cv::Point2f> getPoints( const xmlNodePtr node );
    std::vector<std::vector<cv::Point2f> > getPoints( const std::vector<xmlNodePtr> nodes );
    std::string getTextEquiv( xmlNodePtr node, const char* xpath = ".", const char* separator = " " );
    void registerChange( const char* tool, const char* ref = NULL );
    xmlNodePtr setProperty( xmlNodePtr node, const char* key, const char* val = NULL );
    xmlNodePtr setTextEquiv( xmlNodePtr node,   const char* text, const double* _conf = NULL );
    xmlNodePtr setTextEquiv( const char* xpath, const char* text, const double* _conf = NULL );
    xmlNodePtr setCoords( xmlNodePtr node,   const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePtr setCoords( const char* xpath, const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePtr setBaseline( xmlNodePtr node,   const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePtr setBaseline( const char* xpath, const std::vector<cv::Point2f>& points, const double* _conf = NULL );
    xmlNodePtr addGlyph( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addGlyph( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addWord( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addWord( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextLine( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextLine( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextRegion( xmlNodePtr node, const char* id = NULL, const char* before_id = NULL );
    xmlNodePtr addTextRegion( const char* xpath, const char* id = NULL, const char* before_id = NULL );
    char* getBase();
    int write( const char* fname = "-" );
#if defined (__PAGEXML_OGR__)
    OGRMultiPolygon* getOGRpolygon( const xmlNodePtr node );
#endif
    xmlDocPtr getDocPtr();
    unsigned int getWidth();
    unsigned int getHeight();
  private:
    bool indent = true;
    bool grayimg = false;
    bool extended_names = false;
    char* pagens = NULL;
    xmlNsPtr rpagens = NULL;
    char* xmldir = NULL;
    char* imgpath = NULL;
    char* imgbase = NULL;
    xmlDocPtr xml = NULL;
    xmlXPathContextPtr context = NULL;
    xmlNodePtr rootnode = NULL;
    PageImage pageimg;
    unsigned int width;
    unsigned int height;
    void release();
    void setupXml();
};

#endif
