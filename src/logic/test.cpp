#include "test.h"

#include "cassert"

bool Test::init()
{
    generate_msg3();

    handle_shared_msg1();
    handle_shared_msg1();
    handle_shared_msg1();
    check_seq();

    handle_msg1();
    handle_msg2();
    handle_msg3();

    handle_temp_msg1();
    handle_temp_msg2();
    handle_temp_msg3();

    generate_msg1_ = std::thread(&Test::generate_msg1, this);
    generate_msg2_ = std::thread(&Test::generate_msg2, this);

    return true;
}

void Test::generate_msg1()
{
    uint64_t id = 0;
    while (true)
    {
        InternalMessage imsg;
        imsg.name = "msg1";
        imsg.data = std::to_string(++id);
        assert(message_bus_->push_message(std::move(imsg)));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (id >= 100000000)
        {
            break;
        }
    }
}
void Test::generate_msg2()
{
    uint64_t id = 0;
    while (true)
    {
        InternalMessage imsg;
        imsg.name = "msg2";
        imsg.data = std::to_string(++id);
        assert(message_bus_->push_message(std::move(imsg)));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (id >= 100000000)
        {
            break;
        }
    }
}
CoTask Test::generate_msg3()
{
    uint64_t id = 0;
    auto await = timer_mgr_->create_time_await(10);
    while (true)
    {
        auto r = co_await await;
        InternalMessage imsg;
        imsg.name = "msg3";
        imsg.data = std::to_string(++id);
        assert(message_bus_->push_message(std::move(imsg)));
    }
}

CoTask Test::handle_shared_msg1()
{
    auto await = shared_message_await("msg1");
    while (true)
    {
        auto msg = co_await await;
        assert(msg.name == "msg1");
        {
            uint64_t id = String2Int<uint64_t>(msg.data);
            if (id % 10000 == 0)
            {
                std::cout << __func__ << " handle msg1 id " << id << std::endl;
            }
            std::lock_guard lk(nums_mutex_);
            assert(!nums_.contains(id));
            nums_.insert(id);
        }
    }
}

CoTask Test::check_seq()
{
    auto await = timer_mgr_->create_time_await(1000);
    uint64_t seq = 1;
    while (true)
    {
        auto r = co_await await;
        unordered_set<uint64_t> nums_copy;
        {
            std::lock_guard lk(nums_mutex_);
            nums_copy = nums_;
        }

        while (true)
        {
            if (nums_copy.contains(seq))
            {
                ++seq;
            } else
            {
                break;
            }
        }
        std::cout << "check msg1 seq: " << seq - 1 << std::endl;
    }
}

CoTask Test::handle_msg1()
{
    auto await = message_await("msg1");
    uint64_t last_id = 0;
    while (true)
    {
        auto msg = co_await await;
        assert(msg.name == "msg1");
        uint64_t id = String2Int<uint64_t>(msg.data);
        assert(last_id + 1 == id);
        last_id = id;

        if (id % 10000 == 0)
        {
            std::cout << __func__ << " handle msg1 id " << id << std::endl;
        }
    }
}

CoTask Test::handle_msg2()
{
    auto await = message_await("msg2");
    uint64_t last_id = 0;
    while (true)
    {
        auto msg = co_await await;
        assert(msg.name == "msg2");
        uint64_t id = String2Int<uint64_t>(msg.data);
        assert(last_id + 1 == id);
        last_id = id;

        if (id % 10000 == 0)
        {
            std::cout << __func__ << " handle msg2 id " << id << std::endl;
        }
    }
}

CoTask Test::handle_msg3()
{
    auto await = message_await("msg3");
    //uint64_t last_id = 0;
    while (true)
    {
        auto msg = co_await await;
        assert(msg.name == "msg3");
        uint64_t id = String2Int<uint64_t>(msg.data);
        // assert(last_id + 1 == id);
        //last_id = id;

        if (id % 1000 == 0)
        {
            std::cout << __func__ << " handle msg3 id " << id << std::endl;
        }
    }
}

CoTask Test::handle_temp_msg1()
{
    auto await = temp_message_await("msg1");
    while (true)
    {
        auto msg = co_await await;
        assert(msg.name == "msg1");
        uint64_t id = String2Int<uint64_t>(msg.data);
        if (id % 10000 == 0)
        {
            std::cout << __func__ << " handle msg1 id " << id << std::endl;
        }
    }
}

CoTask Test::handle_temp_msg2()
{
    auto await = temp_message_await("msg2");
    while (true)
    {
        auto msg = co_await await;
        assert(msg.name == "msg2");
        uint64_t id = String2Int<uint64_t>(msg.data);
        if (id % 10000 == 0)
        {
            std::cout << __func__ << " handle msg2 id " << id << std::endl;
        }
    }
}

CoTask Test::handle_temp_msg3()
{
    auto await = temp_message_await("msg3");
    while (true)
    {
        auto msg = co_await await;
        assert(msg.name == "msg3");
        uint64_t id = String2Int<uint64_t>(msg.data);
        if (id % 1000 == 0)
        {
            std::cout << __func__ << " handle msg3 id " << id << std::endl;
        }
    }
}