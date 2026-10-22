## perf优化之前
> 本文档只展示部分着重突出timerwheel的表现
> 由此可见：优化后，时间轮不再是主要用户态 CPU 热点。
# 优化TimerWheel之前
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 504  of event 'cpu-clock:u'
# Event count (approx.): 2532663000
#
# Overhead  Command      Shared Object        Symbol                                                                                                                                                                                                                
    21.63%  http_server  http_server          [.] TimerWheel::TimerRefreshInLoop(unsigned long)
            |
            ---TimerWheel::TimerRefreshInLoop(unsigned long)
               |          
                --21.43%--std::_Function_handler<void (), TimerWheel::TimerRefresh(unsigned long)::{lambda()#1}>::_M_invoke(std::_Any_data const&)
                          TimerWheel::TimerRefresh(unsigned long)
                          Connection::HandleAnyEvent()
                          Channel::HandleEvent()
                          EventLoop::Start()
                          LoopThread::ThreadEntry()
                          0x7e9c076ecdb4
                          start_thread
                          clone3
## 优化TimerWheel之后
# To display the perf.data header info, please use --header/--header-only options.
#
#
# Total Lost Samples: 0
#
# Samples: 408  of event 'cpu-clock:u'
# Event count (approx.): 2050251000
#
# Overhead  Command      Shared Object        Symbol              
            |       |--0.25%--std::__new_allocator<std::__detail::_Hash_node<std::pair<unsigned long const, TimerWheel::TimePosition>, false> >::allocate(unsigned long, void const*) (inlined)
            |          |          std::allocator<std::__detail::_Hash_node<std::pair<unsigned long const, TimerWheel::TimePosition>, false> >::allocate(unsigned long) (inlined)
            |          |          std::allocator_traits<std::allocator<std::__detail::_Hash_node<std::pair<unsigned long const, TimerWheel::TimePosition>, false> > >::allocate(std::allocator<std::__detail::_Hash_node<std::pair<unsigned long const, TimerWheel::TimePosition>, false> >&, unsigned long) (inlined)
            |          |          std::__detail::_Hash_node<std::pair<unsigned long const, TimerWheel::TimePosition>, false>* std::__detail::_Hashtable_alloc<std::allocator<std::__detail::_Hash_node<std::pair<unsigned long const, TimerWheel::TimePosition>, false> > >::_M_allocate_node<unsigned long&, TimerWheel::TimePosition>(unsigned long&, TimerWheel::TimePosition&&) (inlined)
            |          |          std::_Hashtable<unsigned long, std::pair<unsigned long const, TimerWheel::TimePosition>, std::allocator<std::pair<unsigned long const, TimerWheel::TimePosition> >, std::__detail::_Select1st, std::equal_to<unsigned long>, std::hash<unsigned long>, std::__detail::_Mod_range_hashing, std::__detail::_Default_ranged_hash, std::__detail::_Prime_rehash_policy, std::__detail::_Hashtable_traits<false, false, true> >::_Scoped_node::_Scoped_node<unsigned long&, TimerWheel::TimePosition>(std::__detail::_Hashtable_alloc<std::allocator<std::__detail::_Hash_node<std::pair<unsigned long const, TimerWheel::TimePosition>, false> > >*, unsigned long&, TimerWheel::TimePosition&&) (inlined)
            |          |          std::pair<std::__detail::_Node_iterator<std::pair<unsigned long const, TimerWheel::TimePosition>, false, false>, bool> std::_Hashtable<unsigned long, std::pair<unsigned long const, TimerWheel::TimePosition>, std::allocator<std::pair<unsigned long const, TimerWheel::TimePosition> >, std::__detail::_Select1st, std::equal_to<unsigned long>, std::hash<unsigned long>, std::__detail::_Mod_range_hashing, std::__detail::_Default_ranged_hash, std::__detail::_Prime_rehash_policy, std::__detail::_Hashtable_traits<false, false, true> >::_M_emplace<unsigned long&, TimerWheel::TimePosition>(std::integral_constant<bool, true>, unsigned long&, TimerWheel::TimePosition&&) (inlined)
            |          |          std::pair<std::__detail::_Node_iterator<std::pair<unsigned long const, TimerWheel::TimePosition>, false, false>, bool> std::_Hashtable<unsigned long, std::pair<unsigned long const, TimerWheel::TimePosition>, std::allocator<std::pair<unsigned long const, TimerWheel::TimePosition> >, std::__detail::_Select1st, std::equal_to<unsigned long>, std::hash<unsigned long>, std::__detail::_Mod_range_hashing, std::__detail::_Default_ranged_hash, std::__detail::_Prime_rehash_policy, std::__detail::_Hashtable_traits<false, false, true> >::emplace<unsigned long&, TimerWheel::TimePosition>(unsigned long&, TimerWheel::TimePosition&&) (inlined)
            |          |          std::pair<std::__detail::_Node_iterator<std::pair<unsigned long const, TimerWheel::TimePosition>, false, false>, bool> std::unordered_map<unsigned long, TimerWheel::TimePosition, std::hash<unsigned long>, std::equal_to<unsigned long>, std::allocator<std::pair<unsigned long const, TimerWheel::TimePosition> > >::emplace<unsigned long&, TimerWheel::TimePosition>(unsigned long&, TimerWheel::TimePosition&&) (inlined)
            |          |          TimerWheel::TimerRefreshInLoop(unsigned long)
            |          |          std::_Function_handler<void (), TimerWheel::TimerRefresh(unsigned long)::{lambda()#1}>::_M_invoke(std::_Any_data const&)
            |          |          __invoke_impl<void, TimerWheel::TimerRefresh(uint64_t)::<lambda()>&> (inlined)
            |          |          __invoke_r<void, TimerWheel::TimerRefresh(uint64_t)::<lambda()>&> (inlined)
            |          |          _M_invoke (inlined)
            |          |          TimerWheel::TimerRefresh(unsigned long)
            |          |          EventLoop::TimerRefresh(unsigned long) (inlined)
            |          |          Connection::HandleAnyEvent()
            |          |          std::function<void ()>::operator()() const (inlined)
            |          |          Channel::HandleEvent()
            |          |          EventLoop::Start()
            |          |          HttpServer::Listen() (inlined)
            |          |          main
            |          |          0xffffffffffffffff


#
# (Cannot load tips.txt file, please install perf!)
#
