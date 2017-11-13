




// 初始化线程池
tk_threadpool_t *tpool_init(int thread_num){
    // 分配线程池
    tk_threadpool_t* pool;
    if((pool = (tk_threadpool_t *)malloc(sizeof(tk_threadpool_t))) == NULL)
        goto err;

    // threads指针指向线程数组（存放tid），数组大小即为线程数
    pool->thread_count = 0;
    pool->queue_size = 0;
    pool->shutdown = 0;
    pool->started = 0;
    pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * thread_num);
    
    // 分配并初始化task头结点
    pool->head = (tk_task_t*)malloc(sizeof(tk_task_t));
    if((pool->threads == NULL) || (pool->head == NULL))
        goto err;
    pool->head->func = NULL;
    pool->head->arg = NULL;
    pool->head->next = NULL;

    // 初始化锁
    if(pthread_mutex_init(&(pool->lock), NULL) != 0)
        goto err;

    // 初始化条件变量
    if(pthread_cond_init(&(pool->cond), NULL) != 0)
        goto err;

    // 创建线程
    for(int i = 0; i < thread_num; ++i){
        if(pthread_create(&(pool->threads[i]), NULL, threadpool_worker, (void*)pool) != 0){
            threadpool_destory(pool, 0);
            return NULL;
        }
        pool->thread_count++;
        pool->started++;
    }
    return pool;

err:
    if(pool)
        threadpool_free(pool);
    return NULL;
}