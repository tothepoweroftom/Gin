/*==============================================================================
 
 Copyright 2018 by Roland Rabien
 For more information visit www.rabiensoftware.com
 
 ==============================================================================*/

class SystemSemaphore::Impl
{
public:
    Impl (String name)
    {
        String shareName = "/jsem" + File::createLegalFileName (name);
        sem = sem_open (shareName.toRawUTF8(), O_CREAT, 0644, 1);
        
        jassert (sem != SEM_FAILED);
    }
    
    ~Impl()
    {
        sem_close (sem);
    }
    
    bool lock()
    {
        if (! locked)
        {
            sem_wait (sem);
            locked = true;
            return true;
        }
        return false;
    }
    
    bool unlock()
    {
        if (locked)
        {
            sem_post (sem);
            locked = false;
            return true;
        }
        return false;
    }
    
    bool isValid()
    {
        return sem != SEM_FAILED;
    }
    
    sem_t* sem = SEM_FAILED;
    bool locked = false;
};

SystemSemaphore::SystemSemaphore (const String& name)
{
    impl = new Impl (name);
}

SystemSemaphore::~SystemSemaphore()
{
}

bool SystemSemaphore::lock()
{
    return impl->lock();
}

bool SystemSemaphore::unlock()
{
    return impl->unlock();
}

bool SystemSemaphore::isValid()
{
    return impl->isValid();
}
