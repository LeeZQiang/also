#pragma once

#include <algorithm>
#include <mockturtle/mockturtle.hpp>
#include <percy/percy.hpp>
#include <string>
#include <sstream>

using namespace percy;

namespace also
{

  template<typename Out>
    void split_by_delim(const std::string &s, char delim, Out result) 
    {
      std::stringstream ss(s);
      std::string item;
      while (std::getline(ss, item, delim)) {
        *(result++) = item;
      }
    }

  std::vector<std::string> split_by_delim(const std::string &s, char delim) 
  {
    std::vector<std::string> elems;
    split_by_delim(s, delim, std::back_inserter(elems));
    return elems;
  }

    template <typename Iterator>
      inline bool next_combination(const Iterator first, Iterator k, const Iterator last)
      {
        /* Credits: Thomas Draper */
        if ((first == last) || (first == k) || (last == k))
          return false;
        Iterator itr1 = first;
        Iterator itr2 = last;
        ++itr1;
        if (last == itr1)
          return false;
        itr1 = last;
        --itr1;
        itr1 = k;
        --itr2;
        while (first != itr1)
        {
          if (*--itr1 < *itr2)
          {
            Iterator j = k;
            while (!(*itr1 < *j)) ++j;
            std::iter_swap(itr1,j);
            ++itr1;
            ++j;
            itr2 = k;
            std::rotate(itr1,j,last);
            while (last != j)
            {
              ++j;
              ++itr2;
            }
            std::rotate(k,itr2,last);
            return true;
          }
        }   
        std::rotate(first,k,last);
        return false;
      }
    
    std::vector<std::vector<unsigned>> get_all_combination_index( std::vector<unsigned>& vars,
                                                                  const unsigned& n,
                                                                  const unsigned& k )
    {
      std::vector<std::vector<unsigned>> index;
      std::vector<unsigned> tmp;

      do
      {
        tmp.clear();
        for ( unsigned i = 0; i < k; ++i )
        {
          tmp.push_back( vars[i] );
        }
        index.push_back( tmp );
      }
      while( next_combination( vars.begin(), vars.begin() + k, vars.end() ) );

      return index;
    }
      
    template <typename T>
      void show_array( const std::vector<T>& array )
      {
        std::cout << "Elements: ";
        for( const auto& x : array )
        {
          std::cout << " " << x;
        }
        std::cout << std::endl;
      }

    void print_sat_clause(solver_wrapper* solver, pabc::lit* begin, pabc::lit* end)
    {
      printf("Add clause:  " );
      pabc::lit* i;
      for ( i = begin; i < end; i++ )
          printf( "%s%d ", (*i)&1 ? "!":"", (*i)>>1 );
      printf( "\n" );
    }
    
    int add_print_clause(std::vector<std::vector<int>>& clauses, pabc::lit* begin, pabc::lit* end) 
    {
      std::vector<int> clause;
      while (begin != end) {
        clause.push_back(*begin);
        begin++;
      }
      clauses.push_back(clause);

      return 1;
    }
    
    std::vector<std::string> split(const std::string& s, char delimiter)
    {
      std::vector<std::string> tokens;
      std::string token;
      std::istringstream tokenStream(s);
      while (std::getline(tokenStream, token, delimiter))
      {
        tokens.push_back(token);
        std::cout << token << std::endl;
      }
      return tokens;
    }
    
    void to_dimacs(FILE* f, solver_wrapper* solver, std::vector<std::vector<int>>& clauses ) 
    {
      fprintf(f, "p cnf %d %d\n", solver->nr_vars(), clauses.size() );
      for (const auto& clause : clauses) {
        for (const auto lit : clause) {
          // NOTE: variable 0 does not exist in DIMACS format
          const auto var = pabc::Abc_Lit2Var(lit) + 1;
          const auto is_c = pabc::Abc_LitIsCompl(lit);
          fprintf(f, "%d ", is_c ? -var : var);
        }
        fprintf(f, "0\n");
      }
    }
    
    void print_stats( const xmg_network& xmg )
    {
      depth_view depth_xmg( xmg );
      std::cout << fmt::format( "xmg   i/o = {}/{}   gates = {}   level = {}\n", 
          xmg.num_pis(), xmg.num_pos(), xmg.num_gates(), depth_xmg.depth() );
    }
}

