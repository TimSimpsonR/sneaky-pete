
#include "nova/guest/apt.h"

#include <apt-pkg/acquire.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/cacheiterators.h>
#include <list>
#include <sstream>
#include <boost/foreach.hpp>

#include "nova/log.h"

#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/cachefile.h>
//#include <apt-pkg/cacheset.h>
#include <apt-pkg/init.h>

//pkgCache - apt-pkg/pkgcache.h
//MMap - apt-pkg/mmap.h

#include <apt-pkg/configuration.h>

namespace nova { namespace guest { namespace apt {

class Status : public pkgAcquireStatus {

    Log log;

    virtual bool MediaChange(string Media,string Drive) {
        log.debug("Media Change? Uh-oh, this won't end well... :(");
        return false;
    }

    virtual void IMSHit(pkgAcquire::ItemDesc & item) {
      log.debug("Item is up to date: URI:%s ShortDesc:%s Desc: %s",
                item.URI.c_str(), item.ShortDesc.c_str(),
                item.Description.c_str());
    }
   virtual void Fetch(pkgAcquire::ItemDesc & item) {
     log.debug("Fetched: URI:%s ShortDesc:%s Desc: %s",
                item.URI.c_str(), item.ShortDesc.c_str(),
                item.Description.c_str());
   }

   virtual void Done(pkgAcquire::ItemDesc & item) {
     log.debug("Done: URI:%s ShortDesc:%s Desc: %s",
                item.URI.c_str(), item.ShortDesc.c_str(),
                item.Description.c_str());
   }

   virtual void Fail(pkgAcquire::ItemDesc & item) {
     log.error2("FAILED!: URI:%s ShortDesc:%s Desc: %s",
                item.URI.c_str(), item.ShortDesc.c_str(),
                item.Description.c_str());
   }

   virtual void Start() {
     log.debug("The acquire process has begun!");
   }

   virtual void Stop() {
     log.debug("The acquire process has ceased...");
   }
};

class Cache {

    public:
        Cache()
        : cache() {
            global_init();
            OpTextProgress progress(*_config);
            if (cache.Open(progress, true) == false) {
              throw AptException(AptException::OPEN_FAILED);
            }
        }

        ~Cache() {
        }

        template<typename ListType>
        inline void all_versions(ListType & return_list, const char * pkg_name)
        {
            pkgCache::PkgIterator package = cache->FindPkg(pkg_name);
            for (pkgCache::VerIterator itr = package.VersionList();
                 itr.IsGood(); itr ++)
            {
                return_list.push_back(itr.VerStr());
            }
        }

        static void global_init() {
            static bool initialized = false;
            if (!initialized) {
                // If this isn't set opening fails. (...?)
                _config->Set("Debug::NoLocking",true);
                if (pkgInitConfig(*_config) == false) {
                    throw AptException(AptException::INIT_CONFIG_FAILED);
                }
                if (pkgInitSystem(*_config,_system) == false) {
                    throw AptException(AptException::INIT_SYSTEM_FAILED);
                }
                initialized = true;
            }
        }

        void install(const char * package_name) {
            pkgCache & inner_cache = *cache;
            pkgDepCache dep(&(inner_cache));
            OpTextProgress progress(*_config);
            dep.Init(&progress);
            pkgCache::PkgIterator package = cache->FindPkg(package_name);
            dep.MarkInstall(package);
            //dep.MarkAndSweep();
            //dep.Update();
            //dep.Sweep();

            // Look at DoBuildDep
            // Create the text record parsers
            //pkgSourceList * list = cache.GetSourceList();
            //pkgRecords records(cache);
            //pkgSrcRecords source_records(*list);
            //if (_error->PendingError() == true)
            //    throw AptException(AptException::GENERAL);

            // Create the download object
            Status status;
            //AcqTextStatus status(ScreenWidth,_config->FindI("quiet",0));
            pkgAcquire fetcher(&status);

            bool stripMultiArch = StripMultiArch();

            pkgSrcRecords::Parser *Last = FindSrc(*I,Recs,SrcRecs,Src,*Cache);

        }

        static bool StripMultiArch() {
            string hostArch = _config->Find("APT::Get::Host-Architecture");
            if (hostArch.empty() == false)
            {
                // Assume everything is happy. :D
                //std::vector<std::string> archs = APT::Configuration::getArchitectures();
                //if (std::find(archs.begin(), archs.end(), hostArch) == archs.end())
                //    throw AptException(AptException::NO_ARCH_INFO);
                return false;
            }
            else
                return true;
        }

        const char * version(const char * package_name) {
            pkgCache::PkgIterator p = cache->FindPkg(package_name);
            return p.CurVersion();
        }

    private:
        pkgCacheFile cache;
};

class CacheFile : public pkgCacheFile
{
   static pkgCache *SortCache;
   static int NameComp(const void *a,const void *b);

   public:
   pkgCache::Package **List;

   void Sort();
   bool CheckDeps(bool AllowBroken = false);
   bool BuildCaches(bool WithLock = true)
   {
        OpTextProgress Prog(*_config);
      if (pkgCacheFile::BuildCaches(Prog,WithLock) == false)
        return false;
      return true;
   }
   bool Open(bool WithLock = true)
   {
      OpTextProgress Prog(*_config);
        if (pkgCacheFile::Open(Prog,WithLock) == false)
            return false;
      //Sort();

      return true;
   };
   bool OpenForInstall()
   {
      if (_config->FindB("APT::Get::Print-URIs") == true)
	 return Open(false);
      else
	 return Open(true);
   }
   CacheFile() : List(0) {};
   ~CacheFile() {
       if (List) {
        delete[] List;
       }
   }
};


void install(const char * package_name, int time_out) {

// I think to do this I'll need to grab the PkgIterator, then change the state
// to Installed.
// itr-> gets me the package, then itr->

// Theory 2
// Use a pkgDepCache, and call MarkPackage with the pkgCache::PkgIterator for
// what you want, plus a pkgCache::VerIterator.
// Nevermind, there is also a MarkInstall
//   void MarkInstall(PkgIterator const &Pkg,bool AutoInst = true,
//                    unsigned long Depth = 0, bool FromUser = true,
//                    bool ForceImportantDeps = false);
    Cache cache;
    cache.install(package_name);

    #ifdef HRU
    Log log;

    Cache cache;
    log.debug("Version of cowsay is %s", cache.version("cowsay"));
    std::vector<std::string> versions;
    cache.all_versions<std::vector<std::string> >(versions, "cowsay");
    log.debug("Available versions:");
    BOOST_FOREACH(const std::string & version, versions) {
        log.debug("\t%s", version.c_str());
    }
    #endif
    #ifdef BANANAS

    // Initialization
    // If this isn't set opening fails. (?)
    _config->Set("Debug::NoLocking",true);
    if (pkgInitConfig(*_config) == false) {
        log.debug(":(");
        return;
    }
    if (pkgInitSystem(*_config,_system) == false) {
        log.debug(":( 2");
        return;
    }

    OpTextProgress progress(*_config);

    //CacheFile cache;
    pkgCacheFile cache;
    if (cache.Open(progress, true) == false) {
      log.debug("uh oh");
      return;
    }
/*
    if (cache.OpenForInstall() == false) {
      log.debug("uh oh 2");
      return;
    }*/
    //if ( cache.CheckDeps() == false) {
    //    log.debug("uh oh 3");
    //    return;
    //}

    //pkgCacheFile * f = &f;
    //pkgCache * c = f->GetPkgCache();

    pkgCache::PkgIterator p = cache->FindPkg("cowsay");

    {
        const char * name = p.Name();
        log.debug("Package name is %s", name);
        for (pkgCache::VerIterator itr = p.VersionList(); itr.IsGood(); itr ++){
            log.debug("version %s", itr.VerStr());
        }
        log.debug("Current version: %s",
                  (p.CurVersion() == 0 ? "NULL" : p.CurVersion()));
        /*
        const char * ver = p.CurrentVer().Arch(); //.VerStr();
        std::stringstream msg;
        msg << "Name: " << name;
        msg << " Version: " << ver;
        if (!p.CurrentVer().IsGood() || ver == 0 || ver == NULL) {
            log.error("NOOO!!! ");
        } else {
            //log.debug("Package name is %s. Version is %s.", name, ver);
            log.info(msg.str());
        }*/
    }
    /*for(pkgCache::PkgIterator itr = cache->PkgBegin();
        itr != cache->PkgEnd(); itr ++)
    {

    }*/
    log.debug("We win.");
    #endif
#ifdef BLAH
    unsigned long map_flags = 0;
    MMap map(map_flags);

    // Create a pkgDepCache
    //pkgDepCache(pkgCache *Cache,Policy *Plcy = 0);
    pkgCache cache(&map,
                   false /*DoMap */);

    for(pkgCache::PkgIterator itr = cache.PkgBegin();
        itr != cache.PkgEnd(); itr ++)
    {

    }
#endif



/*
    pkgDepCache::Policy * policy = 0;
    log.debug("Hi");
  //  pkgDepCache dep_cache(cache, policy);
//log.debug("Hi2");

    OpProgress progress;
    bool with_lock=false;


    cache.Open(progress, lock);


    // I think MarkInstall is the func to call on the cache...
    //const pkgCache::PkgIterator & Pkg

    //dep_cache.MarkInstall(pkg, true, 5, true, true);

    //PkgIterator
    //bool MarkInstall(PkgIterator const &Pkg,bool AutoInst = true,
	//	    unsigned long Depth = 0, bool FromUser = true,
	//	    bool ForceImportantDeps = false);

   //CacheFile cache;
   //if (Cache.OpenForInstall() == false ||
   //    Cache.CheckDeps(CmdL.FileSize() != 1) == false)
   //   return false;

    */

}

void remove(const char * package_name, int time_out) {
}

std::string version(const char * package_name) {
    Cache cache;
    return cache.version(package_name);
}

} } }  // end namespace nova::guest::apt
