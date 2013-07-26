#include "fastread.h"
#include <boost/tokenizer.hpp>

// [[Rcpp::depends(BH)]]

using namespace Rcpp ;

namespace fastread{
    
    /**
     * Abstraction around Boost::Tokenizer, so that we can use something else
     * without affecting the rest of the code
     */
    class Tokenizer {
    public:
        Tokenizer() : tokenizer_(std::string("")){}
        inline void assign( const std::string& line){ 
            tokenizer_.assign(line) ;
            it = tokenizer_.begin() ;
        } 
        const std::string& get_token(){
            token = *it++; 
            return token ;
        } 
    private:
        typedef boost::tokenizer< boost::escaped_list_separator<char> > BoostTokenizer ;
        typedef BoostTokenizer::const_iterator const_iterator ;
        
        BoostTokenizer tokenizer_ ;
        
        std::string token ;
        const_iterator it ;
    
    } ;
    
    template <typename T>
    class FromString{
    public:
        FromString() : ss(""){} 
        
        // maybe worth using e.g. atoi for int, etc ...
        T get( const std::string& s){
            ss.str( s );
            ss >> value ;
            return value ;
        }
        
    private:
        std::istringstream ss ;
        T value ;
    } ;  
    
    template <>
    class FromString<int>{
    public:
        FromString() {} 
        inline int get( const std::string& s){
            return atoi(s.c_str());
        }
        
    } ;  
    
    template <>
    class FromString<double>{
    public:
        FromString() {} 
        inline double get( const std::string& s){
            return atof(s.c_str());
        }
        
    } ;  
    
    
    template <>
    class FromString< String >{
       public:
           FromString(){}
           String get(const std::string& s){ return String(s) ;}
       
    } ;
    
    class VectorInput_Base {
    public:
        VectorInput_Base(){}
        virtual ~VectorInput_Base(){} ;
        
        virtual void set( int i, const std::string& chunk ) = 0 ;
        virtual SEXP get() = 0; 
    } ;
    
    template <typename Storage, typename Converter>
    class VectorInput : public VectorInput_Base {
    public:
        VectorInput( int n ) : data(n), converter() {}
        
        void set( int i, const std::string& chunk ){
            data[i] = converter.get( chunk ) ;
        }
        SEXP get(){ return data ; }
        
    private:
        Storage data;  
        Converter converter ;
    } ;
    
    typedef VectorInput< IntegerVector   , FromString<int>    > VectorInput_Integer ;
    typedef VectorInput< DoubleVector    , FromString<double> > VectorInput_Double  ;
    typedef VectorInput< CharacterVector , FromString<String> > VectorInput_String  ;
    
    class DataReader {
    public:
        DataReader(int n_) : n(n_), data(), ncol(2) {
            data.push_back( new VectorInput_Integer(n) ) ;
            data.push_back( new VectorInput_Double(n)  ) ;
            // data.push_back( new VectorInput_String(n)  ) ;
        }
        
        void set( int i, const std::string& line ){
            tokenizer.assign(line); 
            for( int j=0; j<ncol; j++){
                data[j]->set( i, tokenizer.get_token() ) ;
            }
        }   
        
        void read_file( const std::string& filename){
            std::string line ;
            std::ifstream file(filename.c_str());
            
            for( int i=0; i<n; i++){
                std::getline(file,line);
                set( i, line );
            }
            file.close();
        }
        
        ~DataReader(){
            // deleting the inputs
            for( int i=0; i<ncol; i++){
                delete data[i] ;    
            }
        }
        
        List get(){
            List out(ncol) ;
            for( int i=0; i<ncol; i++){
                out[i] = data[i]->get() ;    
            }
            return out ;
        }
        
    private:
        int n ;
        std::vector< VectorInput_Base* > data ;
        int ncol ;
        Tokenizer tokenizer ;
    } ; 
    
}

// [[Rcpp::export]]
List read_csv(std::string file, int n ){
    fastread::DataReader reader( n ) ;
    reader.read_file( file ) ;
    return reader.get() ;
}

// [[Rcpp::export]]
SEXP scan_( std::string filename, int n, SEXP what ){
    fastread::VectorInput_Base* input ;
    switch( TYPEOF( what ) ){
    case INTSXP:
        input = new fastread::VectorInput_Integer(n) ;
        break;
    case REALSXP: 
        input = new fastread::VectorInput_Double(n) ;
        break; 
    case STRSXP: 
        input = new fastread::VectorInput_String(n) ;
        break ;
    default:
        stop( "unsupported type" ) ;
    }
    
    fastread::FileReader reader(filename) ;
    for( int i=0; i<n; i++){
        if( reader.is_finished() ) break ;
        input->set( i, reader.get_token() ) ;
    }
    
    SEXP res = input->get() ;
    delete input ;
    
    return res ;
}

