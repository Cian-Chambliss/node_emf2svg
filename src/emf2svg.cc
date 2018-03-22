//int emf2svg(char *contents, size_t length, char **out, size_t *out_length,
//            generatorOptions *options);
#include "emf2svg.h"
#include <node_api.h>
#include <uv.h>
#include <assert.h>
#include <string>
#include <vector>

static napi_value s_workerName = 0;


// Populate a std::String from a NAPI value
static bool getAsString(napi_env env, napi_value value , std::string &result )
{
  napi_status status;
  napi_valuetype type;
  status = napi_typeof(env, value, &type);
  if(status != napi_ok) {
    napi_throw_type_error(env, nullptr, "Error checking arguments");
    return false;    
  }
  if (type != napi_string ) {
    napi_throw_type_error(env, nullptr, "Wrong arguments");
    return false;
  }
  size_t length = 0;
  status = napi_get_value_string_utf8(env, value, NULL, 0, &length); // ex. length = Utf8Length = 11
  assert(status == napi_ok);
  size_t bufsize = length+4;
  char *buf = (char *)calloc(bufsize,1);
  status = napi_get_value_string_utf8(env, value, buf, bufsize, &length); // ex. length = copied = 12
  assert(status == napi_ok);
  result = buf;
  free(buf);
  return true; 
}

static bool getPropertyAsString(napi_env env, napi_value object , const char *name , std::string &result )
{
   napi_value value;
   if( napi_get_named_property( env, object, name , &value ) == napi_ok ) {
      return getAsString(env,value,result);
   }
   return false;
}

static bool getPropertyAsLogical(napi_env env, napi_value object , const char *name ,bool defaultValue)
{
  bool hasProp = false;
  if( napi_has_named_property( env, object,name,&hasProp) == napi_ok )
   {
     if( hasProp )
       {
        napi_value value;
        if( napi_get_named_property( env, object, name , &value ) == napi_ok ) 
        {
          napi_get_value_bool(env,value,&defaultValue);
        }
     }
   }
   return defaultValue;
}

#include <dlfcn.h>

// Convert single EMF file to single SVG file....
typedef int femf2svg_t(const char * ,const char * ,generatorOptions *);
typedef int femf2html_t(int nfilename,const char **infilename, const char * outputfilename, generatorOptions *options);

void* s_lib_handle = NULL;
femf2svg_t *s_femf2svg = NULL;
femf2html_t *s_femf2html = NULL;

static int loadInterface(const char *libraryName)
{
    s_lib_handle = dlopen(libraryName, RTLD_LOCAL|RTLD_LAZY);
    if (!s_lib_handle) 
    {
        printf("Unable to load library\r\n");
    } 
    else 
    {
       s_femf2svg = (femf2svg_t *)dlsym(s_lib_handle, "femf2svg");
       if( !s_femf2svg ) 
       {
          printf("Unable to load symbol 'femf2svg'\r\n");
       }
       s_femf2html = (femf2html_t *)dlsym(s_lib_handle, "femf2html");
       if( !s_femf2html ) 
       {
          printf("Unable to load symbol 'femf2html'\r\n");
       }
       //dlclose(lib_handle);
     }
     return 0;
}
static int call_femf2svg(const char * infile, const char * outputfilename,generatorOptions *options)
{
     if( s_femf2svg )
     {
         return s_femf2svg(infile, outputfilename,options);
     } 
     return 0;
}

static int call_femf2html(int nfilename,const char **infilename, const char * outputfilename,generatorOptions *options)
{
     if( s_femf2html )
     {
         return s_femf2html(nfilename,infilename, outputfilename,options);
     } 
     return 0;
}

class ThreadWorkerContext {
public:
    napi_ref cb_scope_reference;
    napi_ref cb_reference;
    napi_async_work work;
    std::string fnameEmf;
    std::vector< std::string > fnameEmfPages;
    std::string fnameOutput;
    std::string resourcePath;
    std::string error;
    bool        callresult;
    generatorOptions options;
    ThreadWorkerContext() : callresult(false)
    {
      memset(&options,0,sizeof(options));
      options.svgDelimiter = true;
    }
    ~ThreadWorkerContext()
    {
    }
    bool ProcessArguments(napi_env env,const napi_callback_info info)
    { 
      napi_status status;
      size_t argc = 2;
      napi_value args[2];
      status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
      assert(status == napi_ok);
      napi_value emffileValue;
      fnameEmfPages.clear();
      if( napi_get_named_property( env, args[0], "emffile" , &emffileValue ) == napi_ok ) 
      {
          bool isArray = false;
          napi_is_array( env, emffileValue, &isArray);
          if( isArray ) 
          {
            // Populate list of emf files...
            uint32_t count = 0;
            status = napi_get_array_length( env,emffileValue,&count);
            assert(status == napi_ok);
            for( uint32_t i = 0  ; i < count ; ++i )
            {
              napi_value arrayElem;
              status = napi_get_element( env, emffileValue , i , &arrayElem);
              if(status == napi_ok)
              {
                std::string arrayElemString;
                if( getAsString(env,arrayElem,arrayElemString) )
                {
                  fnameEmfPages.push_back(arrayElemString);
                }
              }
            }
            if( fnameEmfPages.size() < 1 )
            {
              napi_throw_type_error(env, nullptr, "No pages in emffile array");
              return false;
            }
            if( !getPropertyAsString(env,args[0],"htmlfile",fnameOutput) ) 
              {
                return false;
              }
          } 
          else if( getAsString(env,emffileValue,fnameEmf) ) 
          {
              if( !getPropertyAsString(env,args[0],"svgfile",fnameOutput) ) 
              {
                return false;
              }
          } else 
          {
              return false;
          }
      }
      status = napi_create_reference(env, args[1], 1, &cb_reference);
      assert(status == napi_ok);
 
      napi_value cb_scope;
      status = napi_get_global(env, &cb_scope);
      assert(status == napi_ok);

      status = napi_create_reference(env, cb_scope, 1, &cb_scope_reference);
      assert(status == napi_ok);

      memset(&options,0,sizeof(options));
      options.svgDelimiter = true;
      options.verbose = getPropertyAsLogical( env, args[0] , "verbose" ,false );
      options.emfplus = getPropertyAsLogical( env, args[0] , "emfplus" ,false );
      options.linkResources = getPropertyAsLogical( env, args[0] , "linkresources" ,false );
      if( options.linkResources )
      {
        if( getPropertyAsString(env,args[0],"resourcepath",resourcePath) )
        {
          options.resourcePath = resourcePath.c_str();
        }
        else
        {
          options.linkResources = false;
        }
      }
      if( !s_lib_handle )
      {
        std::string libraryName;
        if( getPropertyAsString(env,args[0],"library",libraryName) ) 
           {
           loadInterface(libraryName.c_str());
           if( options.verbose && s_femf2svg && s_femf2html )
              {
                printf("Loaded library '%s'\n",libraryName.c_str());
              }
           }
        else
           {
            printf("library name was not specified in call");
           }   
      }
      
      return true;
    }
    void DoWork(napi_env env)
    {
      if( fnameEmfPages.size() > 0  )
      {
        const char ** filenames = (const char ** )malloc(sizeof(const char *) * fnameEmfPages.size() );
        for( size_t i = 0 ; i < fnameEmfPages.size() ; ++i )
        {
          filenames[i] = fnameEmfPages[i].c_str();
        }
        if( call_femf2html( fnameEmfPages.size(), filenames, fnameOutput.c_str(),&options) ) {
          callresult = true;
        } else {
          error = "EMF to HTML convert failed.";
          callresult = false;
        }
        free(filenames);
      }
      else if( call_femf2svg(fnameEmf.c_str(), fnameOutput.c_str(),&options) ) {
        callresult = true;
      } else {
        error = "EMF to SVG convert failed.";
        callresult = false;
      }
    }
    void ProcessReturn(napi_env env)
    {
      napi_status status;
      napi_value argv[2];
      status = napi_get_boolean( env, callresult, &(argv[1]));
      assert(status == napi_ok);
      if( !callresult ) {
          status = napi_create_string_utf8(env, error.c_str(), NAPI_AUTO_LENGTH, &(argv[0]));
      } else {
          status = napi_get_null(env, &(argv[0]));
      }
      assert(status == napi_ok);

      napi_value cb;
      status = napi_get_reference_value(env, cb_reference, &cb);
      assert(status == napi_ok);

      napi_value cb_scope;
      status = napi_get_reference_value(env, cb_scope_reference, &cb_scope);
      assert(status == napi_ok);

      napi_value result;
      status = napi_call_function(env, cb_scope, cb, 2, argv, &result);
      if( status != napi_ok ) {
        fprintf(stderr,"Status = %d\r\n",(int)status);
      }
      assert(status == napi_ok);
      status = napi_delete_reference(env, cb_reference);
      assert(status == napi_ok);
    }
};

static void WorkAsync(napi_env env,void* data)
{
    ThreadWorkerContext *twc = static_cast<ThreadWorkerContext *>(data);
    twc->DoWork(env);
}

static void WorkAsyncComplete(napi_env env, napi_status status,void* data)
{
  ThreadWorkerContext *twc = static_cast<ThreadWorkerContext *>(data);
  twc->ProcessReturn(env);
  napi_delete_async_work(env,twc->work);
  delete twc;
}

napi_value RunCallback(napi_env env, const napi_callback_info info) {
  
  //pthread_t worker_thread;
  ThreadWorkerContext *twc = new ThreadWorkerContext();
  if( twc->ProcessArguments(env,info) ) {
      napi_status status;
      if( !s_workerName )
      {
        napi_create_string_utf8(env, "convertWorker", NAPI_AUTO_LENGTH, &s_workerName);
      }
      status = napi_create_async_work(env,0,s_workerName,WorkAsync,WorkAsyncComplete,twc,&twc->work);
      assert(status == napi_ok);
      status = napi_queue_async_work(env,twc->work);
      assert(status == napi_ok);
  } else {
      delete twc;
  }
  return nullptr;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_value new_exports;
  napi_status status =
      napi_create_function(env, "", NAPI_AUTO_LENGTH, RunCallback, nullptr, &new_exports);
  assert(status == napi_ok);
  return new_exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)