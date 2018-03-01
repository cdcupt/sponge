#ifndef LOG_H
#define LOG_H

include <string.h>

namespace sponge
{
namespace common
{

class Log
{
 public:
  void log(const char *level, const char *filename, const int line, const char *format, ...);
  //返回静态实例
  static Log &get_instance(void);
};

#define SHORT_FILE strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__
#define LOG_INSTANCE (::dstore::common::Log::get_instance())
#define LOG_DEBUG(message, args...) (LOG_INSTANCE.log("DEBUG", SHORT_FILE, __LINE__, message, ##args))
#define LOG_INFO(message, args...) (LOG_INSTANCE.log("INFO", SHORT_FILE, __LINE__, message, ##args))
#define LOG_WARN(message, args...) (LOG_INSTANCE.log("WARN", SHORT_FILE, __LINE__, message, ##args))
#define LOG_ERROR(message, args...) (LOG_INSTANCE.log("ERROR", SHORT_FILE, __LINE__, message, ##args))

} //namespace sponge
} //namespace common

#endif
