#include "rosbag2_cpp/converter_interfaces/serialization_format_converter.hpp"
#include "rosbag2_cpp/rmw_implemented_serialization_format_converter.hpp"
#include "pluginlib/class_list_macros.hpp"

namespace rosbag2_cpp_protobuf_converter
{

class ProtobufConverter : public rosbag2_cpp::converter_interfaces::SerializationFormatConverter
{
public:
  ProtobufConverter() : impl_("protobuf") {}

  ~ProtobufConverter() override = default;

  void serialize(
    std::shared_ptr<const rosbag2_cpp::rosbag2_introspection_message_t> ros_message,
    const rosidl_message_type_support_t * type_support,
    std::shared_ptr<rosbag2_storage::SerializedBagMessage> serialized_message) override
  {
    impl_.serialize(ros_message, type_support, serialized_message);
  }

  void deserialize(
    std::shared_ptr<const rosbag2_storage::SerializedBagMessage> serialized_message,
    const rosidl_message_type_support_t * type_support,
    std::shared_ptr<rosbag2_cpp::rosbag2_introspection_message_t> ros_message) override
  {
    impl_.deserialize(serialized_message, type_support, ros_message);
  }

private:
  rosbag2_cpp::RMWImplementedConverter impl_;
};

}  // namespace rosbag2_cpp_protobuf_converter

PLUGINLIB_EXPORT_CLASS(
  rosbag2_cpp_protobuf_converter::ProtobufConverter, rosbag2_cpp::converter_interfaces::SerializationFormatConverter)
