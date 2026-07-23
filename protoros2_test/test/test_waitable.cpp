#include <gtest/gtest.h>
#include <iostream>
#include "rclcpp/rclcpp.hpp"

class DummyWaitable : public rclcpp::Waitable
{
public:
  void add_to_wait_set(rcl_wait_set_t &) override {}
  bool is_ready(const rcl_wait_set_t &) override { return false; }
  std::shared_ptr<void> take_data() override { return nullptr; }
  std::shared_ptr<void> take_data_by_entity_id(size_t) override { return nullptr; }
  void execute(const std::shared_ptr<void> &) override {}
  void set_on_ready_callback(std::function<void(size_t, int)>) override {}
  void clear_on_ready_callback() override {}
  std::vector<std::shared_ptr<rclcpp::TimerBase>> get_timers() const override { return {}; }
};

class TestWaitable : public ::testing::Test
{
protected:
  static void SetUpTestSuite() { rclcpp::init(0, nullptr); }
  static void TearDownTestSuite() { rclcpp::shutdown(); }
};

TEST_F(TestWaitable, TestWaitSetDestructorClearsWaitable)
{
  auto real_waitable = std::make_shared<DummyWaitable>();

  auto get_waitable = [&]() {
    return std::shared_ptr<rclcpp::Waitable>(real_waitable.get(), [keep_alive = real_waitable](rclcpp::Waitable * w) {
      w->exchange_in_use_by_wait_set_state(false);
    });
  };

  {
    rclcpp::WaitSet wait_set;
    wait_set.add_waitable(get_waitable());
    std::cout << "Waitable added to wait_set" << std::endl;
  }

  std::cout << "WaitSet destroyed" << std::endl;

  {
    rclcpp::WaitSet wait_set2;
    EXPECT_NO_THROW(wait_set2.add_waitable(get_waitable()));
    std::cout << "Waitable added to wait_set2 successfully!" << std::endl;
  }
}
