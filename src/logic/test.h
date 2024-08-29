#pragma once
#include "logic.h"

class Test : public ILogic
{
   public:
    virtual bool init() override;
    using ILogic::ILogic;

   private:
    void generate_msg1();
    void generate_msg2();
    CoTask generate_msg3();

    CoTask handle_shared_msg1();
    unordered_set<uint64_t> nums_;
    mutex nums_mutex_;
    CoTask check_seq();



    CoTask handle_msg1();
    CoTask handle_msg2();
    CoTask handle_msg3();


    CoTask handle_temp_msg1();
    CoTask handle_temp_msg2();
    CoTask handle_temp_msg3();

    std::thread generate_msg1_;
    std::thread generate_msg2_;
};