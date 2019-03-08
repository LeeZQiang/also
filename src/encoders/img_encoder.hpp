#pragma once

#include <vector>
#include <kitty/kitty.hpp>
#include <percy/encoders/encoder.hpp>
#include "../core/misc.hpp"

namespace also
{
  /******************************************************************************
   * class select generate selection variables                                  *
   ******************************************************************************/
  class select_imp
  {
    private:
      int nr_steps;
      int nr_in;

    public:
      select_imp( int nr_steps, int nr_in )
        : nr_steps( nr_steps ), nr_in( nr_in )
      {
      }

      ~select_imp()
      {
      }

      //current step starts from 0, 1, ...
      int get_num_of_sel_vars_for_each_step( int current_step )
      {
        int count = 0;
        int total = nr_in + current_step;
        std::vector<unsigned> idx_array;

        //for total inputs, there are total - 1 combined with 0
        //such as s_0_10, s_0_20, ...
        count += total ;

        for( auto i = 1; i <= total; i++ )
        {
          idx_array.push_back( i );
        }

        //binom( total - 1, 2 ) * 2
        count += get_all_combination_index( idx_array, total, 2u ).size() * 2;

        return count;
      }

      int get_num_sels()
      {
        int count = 0;
        for( auto i = 0; i < nr_steps; i++ )
        {
          count += get_num_of_sel_vars_for_each_step( i );
        }

        return count;
      }

      /* map: 0,     0         _10 
       *      index, nr_steps, inputs ids
       **/
      std::map<int, std::vector<unsigned>> get_sel_var_map()
      {
        std::map<int, std::vector<unsigned>> map;
        std::vector<unsigned> idx_array;
        int count = 0;

        for( auto i = 0; i < nr_steps; i++ )
        {
          int total = nr_in + i;

          //for total inputs, there are total - 1 combined with 0
          //such as s_0_10, s_0_20, ...
          for( auto k = 1; k <= total; k++ )
          {
             std::vector<unsigned> c;
             c.push_back( i );
             c.push_back( k );
             c.push_back( 0 );

             map.insert( std::pair<int, std::vector<unsigned>>( count++, c ) );
          }

          idx_array.clear();
          idx_array.resize( total );
          std::iota( idx_array.begin(), idx_array.end(), 1 );

          //no const & 'a' const 
          for( const auto c : get_all_combination_index( idx_array, idx_array.size(), 2u ) )
          {
            auto tmp = c;
            tmp.insert( tmp.begin(), i );
            map.insert( std::pair<int, std::vector<unsigned>>( count++, tmp ) );

            std::vector<unsigned> reverse;
            reverse.push_back( i );
            reverse.push_back( c[1] );
            reverse.push_back( c[0] );
            map.insert( std::pair<int, std::vector<unsigned>>( count++, reverse ) );
          }
        }

        return map;
      }

  };

  /* public function */
  std::map<int, std::vector<unsigned>> comput_select_imp_vars_map( int nr_steps, int nr_in ) 
  { 
    select_imp s( nr_steps, nr_in );
    return s.get_sel_var_map();
  }

  int comput_select_imp_vars_for_each_step( int nr_steps, int nr_in, int step_idx ) 
  {
    assert( step_idx >= 0 && step_idx < nr_steps );
    select_imp s( nr_steps, nr_in );
    return s.get_num_of_sel_vars_for_each_step( step_idx );
  }

  /******************************************************************************
   * implication logic encoder                                                  *
   ******************************************************************************/
  class img_encoder
  {
    private:
      int nr_sel_vars;
      int nr_sim_vars;

      int sel_offset;
      int sim_offset;
      int op_offset;

      int total_nr_vars;

      bool dirty = false;
      bool print_clause = true;
      bool write_cnf_file = true;

      int num_clauses = 0;
      std::vector<std::vector<int>> clauses;
      
      pabc::lit pLits[2048];
      solver_wrapper* solver;
      FILE* f = NULL;

      std::map<int, std::vector<unsigned>> sel_map;
      int nr_in;
      int tt_size; //local variable if the inputs is less than 3

      /*
       * private functions
       * */
      int get_sim_var( const spec& spec, int step_idx, int t ) const
      {
        return sim_offset + spec.tt_size * step_idx + t;
      }
      
    public:
      img_encoder( solver_wrapper& solver )
      {
        this->solver = &solver;
      }

      ~img_encoder()
      {
      }

      int imply(int a, int b ) const
      {
        int a_bar;
        if( a == 0 ) { a_bar = 1; }
        else if( a == 1 ) { a_bar = 0; }
        else{ assert( false ); }

        return a_bar | b; 
      }

      void create_variables( const spec& spec )
      {
        /* init */
        nr_in = spec.nr_in;
        tt_size = ( ( 1 << nr_in ) - 1 );

        /* number of simulation variables, s_out_in1_in2*/
        sel_map = comput_select_imp_vars_map( spec.nr_steps, spec.nr_in );
        nr_sel_vars = sel_map.size();
        
        /* number of truth table simulation variables */
        nr_sim_vars = spec.nr_steps * spec.tt_size;
        
        /* offsets, this is used to find varibles correspondence */
        sel_offset = 0;
        sim_offset  = nr_sel_vars;

        /* total variables used in SAT formulation */
        total_nr_vars = nr_sel_vars + nr_sim_vars;

        if( spec.verbosity )
        {
          printf( "Creating variables (MIG)\n");
          printf( "nr steps    = %d\n", spec.nr_steps );
          printf( "nr_in       = %d\n", nr_in );
          printf( "nr_sel_vars = %d\n", nr_sel_vars );
          printf( "nr_sim_vars = %d\n", nr_sim_vars );
          printf( "tt_size     = %d\n", tt_size );
          printf( "creating %d total variables\n", total_nr_vars);
        }
        
        /* declare in the solver */
        solver->set_nr_vars(total_nr_vars);
      }
      
      bool encode( const spec& spec )
      {
        if( write_cnf_file )
        {
          f = fopen( "out.cnf", "w" );
          if( f == NULL )
          {
            printf( "Cannot open output cnf file\n" );
            assert( false );
          }
          clauses.clear();
        }

        create_variables( spec );
        create_main_clauses( spec );

        if( !create_fanin_clauses( spec ))
        {
          return false;
        }

        show_variable_correspondence( spec );
        
        if( write_cnf_file )
        {
          to_dimacs( f, solver, clauses );
          fclose( f );
        }

        return true;
      }

      void show_variable_correspondence( const spec& spec )
      {
        printf( "**************************************\n" );
        printf( "selection variables \n");

        for( auto e : sel_map )
        {
          auto array = e.second;
          printf( "s_%d_%d%d is %d\n", array[0], array[1], array[2], e.first );
        }
      }

      bool create_fanin_clauses( const spec& spec )
      {
        auto status = true;

        if (spec.verbosity > 2) 
        {
          printf("Creating fanin clauses (IMPLY)\n");
          printf("Nr. clauses = %d (PRE)\n", solver->nr_clauses());
        }

        int svar = 0;
        for (int i = 0; i < spec.nr_steps; i++) 
        {
          auto ctr = 0;

          auto num_svar_in_current_step = comput_select_imp_vars_for_each_step( spec.nr_steps, spec.nr_in, i ); 
          
          for( int j = svar; j < svar + num_svar_in_current_step; j++ )
          {
            pLits[ctr++] = pabc::Abc_Var2Lit(j, 0);
          }
          
          svar += num_svar_in_current_step;

          status &= solver->add_clause(pLits, pLits + ctr);

          if( print_clause ) { print_sat_clause( solver, pLits, pLits + ctr ); }
          if( write_cnf_file ) { add_print_clause( clauses, pLits, pLits + ctr ); }
        }

        if (spec.verbosity > 2) 
        {
          printf("Nr. clauses = %d (POST)\n", solver->nr_clauses());
        }

        return status;
      }

      /*bool create_op_clauses( const spec& spec )
      {
        bool ret = true;
        for( auto i = 0; i < spec.nr_steps; i++ )
        {
          int ctr = 0;
          pLits[ctr++] = pabc::Abc_Var2Lit( get_op_var( spec, i, 0 ) , 0 );
          
          if( print_clause ) { print_sat_clause( solver, pLits, pLits + ctr ); }
          if( write_cnf_file ) { add_print_clause( clauses, pLits, pLits + ctr ); }
          ret &= solver->add_clause(pLits, pLits + ctr);
        }
        
        assert( ret );
        return ret;
      }*/
      
      bool fix_output_sim_vars(const spec& spec, int t)
      {
        const auto ilast_step = spec.nr_steps - 1;
        auto outbit = kitty::get_bit( spec[spec.synth_func(0)], t + 1);
        
        if ((spec.out_inv >> spec.synth_func(0)) & 1) 
        {
          outbit = 1 - outbit;
        }
        
        const auto sim_var = get_sim_var(spec, ilast_step, t);
        pabc::lit sim_lit = pabc::Abc_Var2Lit(sim_var, 1 - outbit);
        if( print_clause ) { print_sat_clause( solver, &sim_lit, &sim_lit + 1 ); }
        if( write_cnf_file ) { add_print_clause( clauses, &sim_lit, &sim_lit + 1); }
        return solver->add_clause(&sim_lit, &sim_lit + 1);
      }

      /*
       * for the select variable s_i_jk
       * */
      bool add_simulation_clause(
          const spec& spec,
          const int t,
          const int i,
          const int j,
          const int k,
          const int b,
          const int a,
          const int sel_var
          )
      {
        int ctr = 0;
        //assert(j > 0);

        if (j < spec.nr_in) {
          if ((((t + 1) & (1 << j)) ? 1 : 0) != a) {
            return true;
          }
        } else {
          pLits[ctr++] = pabc::Abc_Var2Lit(
              get_sim_var(spec, j - spec.nr_in, t), a);
        }

        if (k < spec.nr_in) {
          if ((((t + 1) & (1 << k)) ? 1 : 0) != b) {
            return true;
          }
        } else {
          pLits[ctr++] = pabc::Abc_Var2Lit(
              get_sim_var(spec, k - spec.nr_in, t), b);
        }

        if (sel_var != -1) {
          pLits[ctr++] = pabc::Abc_Var2Lit(sel_var, 1);
        }

        if ( imply(a, b) ) {
          pLits[ctr++] = pabc::Abc_Var2Lit(get_sim_var(spec, i, t), 0);
        } else {
          pLits[ctr++] = pabc::Abc_Var2Lit(get_sim_var(spec, i, t), 1);
        }

        const auto ret = solver->add_clause(pLits, pLits + ctr);
        if( print_clause ) { print_sat_clause( solver, pLits, pLits + ctr ); }
        assert(ret);
        return ret;
      }

      /*
       * for the select variable s_i_j0
       * */
      bool add_const_simulation_clause(
                                        const spec& spec,
                                        const int t,
                                        const int i,
                                        const int j,
                                        const int a,
                                        const int sel_var
                                      )
      {
        int ctr = 0;
        //assert(j > 0);

        if (j < spec.nr_in) {
          if ((((t + 1) & (1 << j)) ? 1 : 0) != a) {
            return true;
          }
        } else {
          pLits[ctr++] = pabc::Abc_Var2Lit(
              get_sim_var(spec, j - spec.nr_in, t), a);
        }

        if (sel_var != -1) {
          pLits[ctr++] = pabc::Abc_Var2Lit(sel_var, 1);
        }

        if (a) {
          pLits[ctr++] = pabc::Abc_Var2Lit(get_sim_var(spec, i, t), 1);
        } else {
          pLits[ctr++] = pabc::Abc_Var2Lit(get_sim_var(spec, i, t), 0);
        }

        const auto ret = solver->add_clause(pLits, pLits + ctr);
        if( print_clause ) { print_sat_clause( solver, pLits, pLits + ctr ); }
        assert(ret);
        return ret;
      }

      bool create_tt_clauses(const spec& spec, const int t)
      {
        bool ret = true;

        for( const auto svar : sel_map )
        {
          auto s = svar.first;
          auto array  = svar.second;

          auto i = array[0];
          auto j = array[1];
          auto k = array[2];
          
          if( k == 0 )
          {
            ret &= add_const_simulation_clause( spec, t, i, j, 0, s );
            ret &= add_const_simulation_clause( spec, t, i, j, 1, s );
          }
          else
          {
            ret &= add_simulation_clause( spec, t, i, j, k, 0, 0, s );
            ret &= add_simulation_clause( spec, t, i, j, k, 0, 1, s );
            ret &= add_simulation_clause( spec, t, i, j, k, 1, 0, s );
            ret &= add_simulation_clause( spec, t, i, j, k, 1, 1, s );
          }
        }

        ret &= fix_output_sim_vars(spec, t);

        return ret;
      }

      void create_main_clauses( const spec& spec )
      {
        for( int t = 0; t < spec.tt_size; t++ )
        {
          (void) create_tt_clauses( spec, t );
        }
      }

      void print_solver_state(const spec& spec)
      {
        printf("\n");
        printf("========================================"
            "========================================\n");
        printf("  SOLVER STATE\n\n");

        printf("  Nr. variables = %d\n", solver->nr_vars());
        printf("  Nr. clauses = %d\n\n", solver->nr_clauses());

        for( const auto e : sel_map )
        {
          auto svar_idx  = e.first;
          auto step_idx  = e.second[0];
          auto first_in  = e.second[1];
          auto second_in = e.second[2];

          if( solver->var_value( svar_idx ) )
          {
            printf( "x_%d has inputs: x_%d and x_%d\n", step_idx + nr_in + 1, first_in, second_in );
          }
        }

        for( auto i = 0; i < spec.nr_steps; i++ )
        {
          printf( "x_%d performs implication operation \n", i );

          for (int t = spec.get_tt_size() - 1; t >= 0; t--) 
          {
            printf("  x_%d_%d=%d\n", spec.get_nr_in() + i + 1, t + 2, 
                solver->var_value(get_sim_var(spec, i, t) ));
          }
        }

        printf("\n");

        printf("========================================"
            "========================================\n");
      }
      
      void show_verbose_result()
      {
        for( auto i = 0u; i < total_nr_vars; i++ )
        {
          printf( "var %d : %d\n", i, solver->var_value( i ) );
        }
      }
  };
  
  /******************************************************************************
   * Public functions                                                           *
   ******************************************************************************/
  synth_result implication_syn_by_img_encoder( spec& spec, solver_wrapper& solver, img_encoder& encoder )
  {
    spec.verbosity = 2;
    spec.preprocess();
    //encoder.set_dirty( true );
    //encoder.set_print_clause( true );

    spec.nr_steps = spec.initial_steps;

    while( true )
    {
      solver.restart();

      if( !encoder.encode( spec ) )
      {
        spec.nr_steps++;
        break;
      }

      const auto status = solver.solve( spec.conflict_limit );

      if( status == success )
      {
        encoder.show_verbose_result();
        //encoder.extract_mig( spec, mig );
        encoder.print_solver_state( spec );
        return success;
      }
      else if( status == failure )
      {
        spec.nr_steps++;
      }
      else
      {
        return timeout;
      }

    }

    return success;
  }
  
}
