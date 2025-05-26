function(find_assertion_expression_end assert_expr result_remainder)
  #message("${assert_expr}")
  string(FIND "${assert_expr}" "," comma_pos)
  string(FIND "${assert_expr}" "\"" quote_pos)
  string(FIND "${assert_expr}" "(" paren_pos)
  string(LENGTH "${assert_expr}" assert_expr_length)
  set(current_pos 0)
  while((${comma_pos} GREATER -1) AND
        (${paren_pos} GREATER -1 AND ${comma_pos} GREATER ${paren_pos}) OR
        (${quote_pos} GREATER -1 AND ${comma_pos} GREATER ${quote_pos}))
    #message("current_pos: ${current_pos}, comma_pos: ${comma_pos}, paren_pos: ${paren_pos}, quote_pos:${quote_pos}")
    # Ignore comma inside string literal or nested parentheses.
    while((${paren_pos} GREATER -1 OR ${quote_pos} GREATER -1) AND
          (${comma_pos} GREATER ${paren_pos} OR ${comma_pos} GREATER ${quote_pos}))
      #message(" current_pos: ${current_pos}, comma_pos: ${comma_pos}, paren_pos: ${paren_pos}, quote_pos:${quote_pos}")
      set(paren_count 0)
      set(open_quote 0)
      # Determine starting position
      if(${paren_pos} GREATER -1 AND ${quote_pos} GREATER -1)
        if(${paren_pos} LESS ${quote_pos})
          set(current_pos ${paren_pos})
          set(paren_count 1)
        else()
          set(current_pos ${quote_pos})
          set(open_quote 1)
        endif()
      elseif(${paren_pos} GREATER -1)
        set(current_pos ${paren_pos})
        set(paren_count 1)
      elseif(${quote_pos} GREATER -1)
        set(current_pos ${quote_pos})
        set(open_quote 1)
      endif()
      # Scroll outside parenthesis AND string literal
      while((${paren_count} GREATER 0 OR ${open_quote} GREATER 0) AND
            (${current_pos} LESS ${assert_expr_length}))
        #message("  current_pos: ${current_pos}, comma_pos: ${comma_pos}, paren_pos: ${paren_pos}, quote_pos:${quote_pos}")
        # Extract next character
        math(EXPR current_pos "${current_pos} +1")
        string(SUBSTRING "${assert_expr}" ${current_pos} 1 another_byte)
        # Examine next character to be opening or closing of parenthesis or string literal
        if("${another_byte}" STREQUAL ")")
          math(EXPR paren_count "${paren_count} -1")
        elseif("${another_byte}" STREQUAL "(")
          math(EXPR paren_count "${paren_count} +1")
        elseif("${another_byte}" STREQUAL "\"")
          math(EXPR previous_pos "${current_pos} -1")
          string(SUBSTRING "${assert_expr}" ${previous_pos} 1 previous_byte)
          if(NOT "${previous_byte}" STREQUAL "\\")
            if(${open_quote} EQUAL 0)
              set(${open_quote} 1)
            else()
              set(${open_quote} 0)
            endif()
          endif()
        endif()
        #message("  current_pos: ${current_pos}, comma_pos: ${comma_pos}, paren_pos: ${paren_pos}, quote_pos:${quote_pos}")
      endwhile()
      set(paren_pos -1)
      set(quote_pos -1)
      # Scroll outside parenthesis AND string literal
      if(${current_pos} LESS ${assert_expr_length})
        math(EXPR current_pos "${current_pos} +1")
      endif()
    endwhile()
    # By now we closed all parenthesis and string literals or reached end of expression
    string(SUBSTRING "${assert_expr}" ${current_pos} -1 expr_remainder)
    #message(" expr_remainder: >>${expr_remainder}<<")
    if(${comma_pos} LESS ${current_pos})
      # Ignore that comma and look for another one in remainder of the expression
      string(FIND "${expr_remainder}" "," comma_pos)
      if(${comma_pos} GREATER -1)
        math(EXPR comma_pos "${comma_pos} +${current_pos}")
      endif()
    endif()
    # Look for another string literal to contain comma
    string(FIND "${expr_remainder}" "\"" quote_pos)
    if(${quote_pos} GREATER -1)
      math(EXPR quote_pos "${quote_pos} +${current_pos}")
    endif()
    # Look for another parenthesis to contain comma
    string(FIND "${expr_remainder}" "(" paren_pos)
    if(${paren_pos} GREATER -1)
      math(EXPR paren_pos "${paren_pos} +${current_pos}")
    endif()
  endwhile()

  if(${comma_pos} GREATER -1)
    string(SUBSTRING "${assert_expr}" 0 ${comma_pos} post_comma)
    set(${result_remainder} "${post_comma}" PARENT_SCOPE)
  else()
    set(${result_remainder} "${assert_expr}" PARENT_SCOPE)
  endif()

endfunction()

macro(assert_id_generation)
  set(multiValueArgs sources)
  set(option "")
  set(oneValueArgs
    generator_output_path
    verifier_header_path
    wax_inline_path
    verifier_output_path
    namespace)
  cmake_parse_arguments( assertion_id "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

  message("Beginning verification of assertion's given id in ${assertion_id_namespace} namespace")

  set(assert_expressions)
  list(APPEND assert_expressions
  "#include <fc/exception/exception.hpp>\n;"
  "#include <fstream>\n\n;"
  "#define HASH_EXPR( EXPR ) fc::exception::hash_expr( #EXPR )\n\n;"
  "int main( int argc, char** argv ){\n;"
  "  uint64_t h = 0\;\n;"
  "  std::fstream fsv, fsw\;\n;"
  "  fsv.open(\"${assertion_id_verifier_header_path}\", std::ios::out | std::ios::trunc)\;\n;"
  "  fsv << \"#include <cstdint>\" << std::endl\;\n;"
  "  fsw.open(\"${assertion_id_wax_inline_path}\", std::ios::out | std::ios::trunc)\;\n;")
  
  foreach(file ${assertion_id_sources})
    #message("file: ${file}")
    file(REAL_PATH ${file} real_path)
    #message(${real_path})
    file(READ ${real_path} file_contents)
    list(FILTER file_contents INCLUDE REGEX "FC_ASSERT[ \r\n]*\\(" )
    if(NOT "${file_contents}" STREQUAL "")
      get_filename_component(file "${file}" NAME)
      list(APPEND assert_expressions "  fsv << \"//${file}\" << std::endl\;\n")
      foreach(line ${file_contents})
        string(FIND ${line} "FC_ASSERT" start)
        string(FIND ${line} ")" stop REVERSE)
        math(EXPR length "${stop} - ${start} +1")
        string(SUBSTRING ${line} ${start} ${length} assertion)
        #message(">>${assertion}<<")

        # Extract substring of assertion expression
        string(FIND "${assertion}" "(" current_pos)
        string(FIND "${assertion}" ")" stop REVERSE)
        math(EXPR current_pos "${current_pos} +1")
        math(EXPR length "${stop} - ${current_pos}")
        string(SUBSTRING "${assertion}" ${current_pos} ${length} assert_expr)
        #message(">>${assert_expr}<<")

        # Trim assertion message (& its params) if present
        find_assertion_expression_end("${assert_expr}" assert_expr)
        
        string(STRIP "${assert_expr}" trimmed_expr)
        string(REGEX REPLACE "\\\\\"|//|/\\*|\\*/|\\\\\n|\"" "**" trimmed_for_string ${trimmed_expr})
        string(REGEX REPLACE "[\r\n]+" " " trimmed_for_string ${trimmed_for_string})
        string(REGEX REPLACE "\\\\0" "" trimmed_for_string ${trimmed_for_string})
        #message(">>${trimmed_expr}<<")
        list(APPEND assert_expressions
        "  h = HASH_EXPR( ${trimmed_expr} )\;\n;"
        "  fsv << \"uint64_t assertion_\" << h << \" = \" << h << \"ull\; /*${file}*/ /*${trimmed_for_string}*/ \" << std::endl\;\n;"
        "  fsw << \"container.insert( std::make_pair( \" << h << \"ull, \\\"${assertion_id_namespace}\\\") )\;\" << std::endl\;\n;")
      endforeach()
    #message(">> ${file_contents} <<")
    endif()
  endforeach()

  list(APPEND assert_expressions
  "  fsv.close()\;\n;"
  "  fsw.close()\;\n;"
  "  return 0\;\n;"
  "}\n")

message("Creating file ${assertion_id_generator_output_path}_tmp")
set(temp_generator "${assertion_id_generator_output_path}_tmp")
file(WRITE ${temp_generator} ${assert_expressions} )
file(COPY_FILE ${temp_generator} ${assertion_id_generator_output_path} ONLY_IF_DIFFERENT)

set(assert_id_verifier)
list(APPEND assert_id_verifier
"#include \"${assertion_id_verifier_header_path}\"\n;"
"int main( int argc, char** argv ){\n;"
"  return 0\;\n;"
"}\n")
set(temp_verifier "${assertion_id_verifier_output_path}_tmp")
file(WRITE ${temp_verifier} ${assert_id_verifier} )
file(COPY_FILE ${temp_verifier} ${assertion_id_verifier_output_path} ONLY_IF_DIFFERENT)

endmacro()