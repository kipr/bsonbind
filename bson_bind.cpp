#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <cassert>
#include <vector>
#include <unordered_map>
#include <stdexcept>

using namespace std;

namespace
{
  //
  // INPUT
  //
  
  vector<string> &split(const string &s, char delim, vector<string> &elems)
  {
    stringstream ss(s);
    string item;
    elems.clear();
    while(getline(ss, item, delim)) elems.push_back(item);
    return elems;
  }
  
  
  struct member
  {
    bool special;
    bool vec;
    bool required;
    string type;
    string name;
  };
  
  struct conv_member
  {
    bool required;
    bool vec;
    bool ext;
    string type;
    string name;
  };
  
  bool process_line(const string &line, const uint32_t line_num, member &m)
  {
    vector<string> elems;
    split(line, ' ', elems);
    if(elems.empty() || elems.size() > 2)
    {
      cerr << "Expected \"value name\" pair on line " << line_num << endl;
      return false;
    }
    
    if(elems[0].empty())
    {
      cerr << "Type is empty on line " << line_num << endl;
      return false;
    }
    
    if((m.special = elems[0].size() > 1 && elems[0][0] == '%')) elems[0] = elems[0].substr(1);
    if((m.required = elems.size() > 1 && elems[1].size() > 1 && elems[1].back() == '!')) elems[1] = elems[1].substr(0, elems[1].size() - 1);
    if((m.vec = elems[0].size() > 2 && elems[0].substr(elems[0].size() - 2) == "[]")) elems[0] = elems[0].substr(0, elems[0].size() - 2);
    m.type = elems[0];
    m.name = elems.size() > 1 ? elems[1] : "";
    
    return true;
  }
  
  bool process_file(istream &in, vector<member> &ms)
  {
    for(uint32_t line_num = 1;; ++line_num)
    {
      string line;
      getline(in, line);
      if(in.eof() && line.empty()) break;
      if(line.empty() || line[0] == '#') continue;
      member m;
      if(!process_line(line, line_num, m)) return false;
      ms.push_back(m);
    }
    
    return true;
  }
  
  //
  // OUTPUT
  //
  
  struct match_sep
  {
    bool operator()(char ch) const
    {
      return ch == '\\' || ch == '/';
    }
  };
  
  string basename(const string &path)
  {
    return string(find_if(path.rbegin(), path.rend(), match_sep()).base(), path.end());
  }
  
  string remove_extension(const string &file)
  {
    string::const_reverse_iterator pivot = find(file.rbegin(), file.rend(), '.');
    return pivot == file.rend() ? file : string(file.begin(), pivot.base() - 1);
  }
  
  string filename(const string &file)
  {
    return remove_extension(basename(file));
  }
  
  string last(const uint32_t n, const std::string &s)
  {
    return s.substr(s.size() - n);
  }
  
  conv_member convert_member(member m)
  {
    unordered_map<string, string> conversions = {
      {"real32", "float"},
      {"real64", "double"},
      {"int32", "int32_t"},
      {"int16", "int16_t"},
      {"int8", "int8_t"},
      {"bool", "bool"},
      {"uint32", "uint32_t"},
      {"uint16", "uint16_t"},
      {"uint8", "uint8_t"},
      {"string", "std::string"}
    };
    
    auto it = conversions.find(m.type);
    
    conv_member ret;
    ret.required = m.required;
    ret.vec = m.vec;
    ret.ext = it == conversions.end();
    ret.type = ret.ext ? m.type : it->second;
    ret.name = m.name;
    return ret;
  }
  
  vector<conv_member> convert_members(const vector<member> &ms)
  {
    vector<conv_member> ret;
    for(const auto &m : ms) ret.push_back(convert_member(m));
    return ret;
  }
  
  void output_header(ostream &out, const string &name, const vector<conv_member> &ms, const string &package = "bson_bind")
  {
    assert(!name.empty());
    
    out << "// This file is automatically generated! Do not modify!" << endl << endl;
    out << "#ifndef _BSON_BIND_" << name << "_" << endl;
    out << "#define _BSON_BIND_" << name << "_" << endl << endl;
    
    out << "#include <cstdint>" << endl;
    out << "#include <string>" << endl;
    out << "#include <vector>" << endl;
    out << "#include <bson.h>" << endl;
    out << "#include <cstring>" << endl;
    out << "#include <bson_bind/option.hpp>" << endl;
    
    for(const auto &c : ms)
    {
      if(!c.ext) continue;
      out << "#include \"" << c.type << ".hpp\"" << endl;
    }
    
    out << endl;
    
    out << "namespace " << package << " {" << endl;
    out << "  struct " << name << " {" << endl;
  }
  
  void output_member(ostream &out, conv_member m)
  {
    assert(!m.type.empty());
    assert(!m.name.empty());
    
    out << "    ";
    if(!m.required) out << "bson_bind::option<";
    out << (m.vec ? "std::vector<" + m.type + ">" : m.type);
    if(!m.required) out << ">";
    out << " " << m.name << ";" << endl;
  }
  
  string bson_append_primitive(conv_member m, const string key_override = string(), const string value_override = string(), const std::string &doc = string("ret"))
  {
    stringstream out;

    string call = m.name;
    if(!value_override.empty())
    {
      m.name = value_override;
      call = m.name;
    }
    else
    {
      if(!m.required) call += ".unwrap()";
    }
      
    if(m.ext)
    {
      out << "bson_append_document(" << doc << ", " << (key_override.empty() ? "\"" + m.name + "\"" : key_override) << ", -1, " << call << ".bind());";
    }
    else if(m.type == "std::string")
    {
      out << "bson_append_utf8(" << doc << ", " << (key_override.empty() ? "\"" + m.name + "\"" : key_override) <<", -1, " << call << ".c_str(), -1);";
    }
    else
    {
      if(m.type == "int8_t" || m.type == "uint8_t") out << "bson_append_int8";
      else if(m.type[0] == 'i' || m.type[0] == 'u') out << "bson_append_int32";
      else if(m.type == "bool") out << "bson_append_bool";
      else if(m.type == "float" || m.type == "double") out << "bson_append_double";
      out << "(" << doc << ", " << (key_override.empty() ? "\"" + m.name + "\"" : key_override) << ", -1, " << call << ");";
    }
    return out.str();
  }
  
  string bson_type_check(const conv_member &m, bool vec_check, bool negate)
  {
    stringstream out;
    out << "if(v->value_type " << (negate ? "!=" : "==") << " ";
    if(m.ext)
    {
      out << "BSON_TYPE_DOCUMENT";
    }
    else if(vec_check && m.vec && m.type != "uint8_t")
    {
      out << "BSON_TYPE_ARRAY";
    }
    else if(m.vec && m.type == "uint8_t")
    {
      out << "BSON_TYPE_BINARY";
    }
    else if(m.type == "std::string")
    {
      out << "BSON_TYPE_UTF8";
    }
    else
    {
      if(m.type == "int8_t" || m.type == "uint8_t") out << "BSON_TYPE_INT8";
      else if(m.type[0] == 'i' || m.type[0] == 'u') out << "BSON_TYPE_INT32";
      else if(m.type == "bool") out << "BSON_TYPE_BOOL";
      else if(m.type == "float" || m.type == "double") out << "BSON_TYPE_DOUBLE";
    }
    out << ")";
    return out.str();
  }
  
  string bson_read_primitive(conv_member m, const string &r, const string &name_override = string())
  {
    if(!name_override.empty()) m.name = name_override;
    stringstream out;
    if(m.ext)
    {
      out << "d = bson_new_from_data(v->value.v_doc.data, v->value.v_doc.data_len); "
          << (r.empty() ? "" : r + ".") << m.name << " = " << m.type << "::unbind(d); "
          << "bson_destroy(d);";
    }
    else if(m.type == "std::string")
    {
      out << (r.empty() ? "" : r + ".") << m.name << " = std::string(v->value.v_utf8.str, v->value.v_utf8.len);";
    }
    else
    {
      if(m.type == "int8_t" || m.type == "uint8_t") out << (r.empty() ? "" : r + ".") << m.name << " = v->value.v_int8;";
      else if(m.type[0] == 'i' || m.type[0] == 'u') out << (r.empty() ? "" : r + ".") << m.name << " = v->value.v_int32;";
      else if(m.type == "bool") out << (r.empty() ? "" : r + ".") << m.name << " = v->value.v_bool;";
      else if(m.type == "float" || m.type == "double") out << (r.empty() ? "" : r + ".") << m.name << " = v->value.v_double;";
    }
    return out.str();
  }
  
  void output_bind(ostream &out, const vector<conv_member> &ms)
  {
    out << "    bson_t *bind() {" << endl
        << "      bson_t *ret = bson_new();" << endl
        << "      bson_t *arr;" << endl
        << "      uint32_t i = 0;" << endl;
    for(const auto &m : ms)
    {
      if(m.vec && m.type != "uint8_t")
      {
        if(!m.required)
        {
          out << "      if(" << m.name << ".some()) {" << endl;
        }
        out << "      arr = bson_new();" << endl
            << "      i = 0;" << endl
            << "      for(std::vector<" << m.type << ">::const_iterator it = " << m.name << (m.required ? "" : ".unwrap()") << ".begin();" << endl
            << "          it != " << m.name << (m.required ? "" : ".unwrap()") << ".end(); ++it, ++i)" << endl
            << "        " << bson_append_primitive(m, "std::to_string(i).c_str()", "(*it)", "arr") << endl
            << "      bson_append_array(ret, \"" << m.name << "\", -1, arr);" << endl
            << "      bson_destroy(arr);";
        if(!m.required)
        {
          out << endl << "      }";
        }
      }
      else if(m.vec && m.type == "uint8_t")
      {
        if(!m.required)
        {
          out << "      if(" << m.name << ".some()) {" << endl;
        }
        const string call = m.name + (m.required ? "" : ".unwrap()");
        out << "      bson_append_binary(ret, \"" << m.name << "\", -1, BSON_SUBTYPE_BINARY, "
            << call << ".data(), " << call << ".size());" << endl;
        if(!m.required)
        {
          out << endl << "      }";
        }
      }
      else
      {
        if(!m.required)
        {
          out << "      if(" << m.name << ".some()) " << bson_append_primitive(m);
        }
        else
        {
          out << "      " << bson_append_primitive(m);
        }
      }
      out << endl;
    }
    out << "      return ret;" << endl
        << "    }" << endl;
  }
  
  void output_unbind(ostream &out, const vector<conv_member> &ms, const std::string &name)
  {
    out << "    static " << name << " unbind(const bson_t *const bson) {" << endl
        << "      bson_iter_t it;" << endl
        << "      bson_iter_t itt;" << endl
        << "      uint32_t i = 0;" << endl
        << "      bool found;" << endl
        << "      const bson_value_t *v;" << endl
        << "      bson_t *arr;" << endl
        << "      bson_t *d;" << endl
        << "      " << name << " ret;" << endl;
    for(const auto &m : ms)
    {
      out << "      found = bson_iter_init_find(&it, bson, \"" << m.name << "\");" << endl;
      if(m.required)
      {
        out << "      if(!found) throw std::invalid_argument(\"required key " << m.name << " not found in bson document\");" << endl
            << "      else {" << endl;
      }
      else
      {
        out << "      if(found) {" << endl;
      }
      out << "        v = bson_iter_value(&it);" << endl
          << "        " << bson_type_check(m, true,  true) << " throw std::invalid_argument(\"key " << m.name << " has the wrong type\");" << endl;
      if(m.vec && m.type != "uint8_t")
      {
        out << "        arr = bson_new_from_data(v->value.v_doc.data, v->value.v_doc.data_len);" << endl
            << "        i = 0;" << endl;
        if(!m.required)
        {
          string full = m.vec ? "std::vector<" + m.type + ">" : m.type;
          out << "        ret." << m.name << " = bson_bind::some<" << full << " >(" << full << "());" << endl;
        }
        out << "        for(;; ++i) {" << endl
            << "          if(!bson_iter_init_find(&itt, arr, std::to_string(i).c_str())) break;" << endl
            << "          v = bson_iter_value(&itt);" << endl
            << "          " << bson_type_check(m, false, true) << " throw std::invalid_argument(\"key " << m.name << " child has the wrong type\");" << endl
            << "          " << m.type << " tmp;" << endl
            << "          " << bson_read_primitive(m, "", "tmp") << endl
            << "          ret." << m.name << (m.required ? "" : ".unwrap()") << ".push_back(tmp);" << endl
            << "        }" << endl
            << "        bson_destroy(arr);" << endl;
      }
      else if(m.vec && m.type == "uint8_t")
      {
        string full = m.vec ? "std::vector<" + m.type + ">" : m.type;
        if(!m.required)
        {
          out << "        ret." << m.name << " = bson_bind::some(" << full << "(v->value.v_binary.data_len));" << endl;
        }
        else
        {
          out << "        ret." << m.name << ".resize(v->value.v_binary.data_len);" << endl;
        }
        out << "        memcpy(ret." << m.name << ".data(), v->value.v_binary.data, v->value.v_binary.data_len);" << endl;
      }
      else
      {
        out << "        " << bson_read_primitive(m, "ret") << endl;
      }
      out << "      }" << endl;
    }
    out << "      return ret;" << endl
        << "    }" << endl;
  }
  
  void output_default(ostream &out, const string &name)
  {
    out << "    " << name << "() {}" << endl;
  }
  
  void output_copy(ostream &out, const vector<conv_member> &ms, const string &name)
  {
    out << "    " << name << "(const " << name << " &rhs)" << endl;
    bool first = true;
    for(const auto &c : ms)
    {
      out << "      " << (first ? ":" : ",") << " " << c.name << "(rhs." << c.name << ")" << endl;
      first = false;
    }
    out << "      {}" << endl;
  }
  
  void output_assign(ostream &out, const vector<conv_member> &ms, const string &name)
  {
    out << "    " << name << " &operator =(const " << name << " &rhs) {" << endl;
    bool first = true;
    for(const auto &c : ms)
    {
      out << "      " << c.name << " = rhs." << c.name << ";" << endl;
      first = false;
    }
    out << "      return *this;" << endl
        << "    }" << endl;
  }
  
  void output_footer(ostream &out)
  {
    out << "  };" << endl;
    out << "}" << endl << endl;
    out << "#endif" << endl;
  }
  
  void output_file(ostream &out, const vector<member> &ms, const string &name)
  {
    const auto realname = filename(name);
    string package = "bson_bind";
    bool gen_bind = true;
    bool gen_unbind = true;
    bool gen_copy = true;
    bool gen_assign = true;
    auto rms = ms;
    for(auto it = rms.begin(); it != rms.end();)
    {
      if(it->special)
      {
        if(it->type == "package") package = it->name;
        else if(it->type == "nobind") gen_bind = false;
        else if(it->type == "nounbind") gen_unbind = false;
        else if(it->type == "nocopy") gen_copy = false;
        else if(it->type == "noassign") gen_assign = false;
        else
        {
          cerr << "Warning: unrecognized directive " << it->type << endl;
        }
        it = rms.erase(it);
        continue;
      }
      ++it;
    }
    const auto conv = convert_members(rms);
    
    output_header(out, realname, conv, package);
    for(const auto &c : conv) output_member(out, c);
    if(gen_bind) output_bind(out, conv);
    if(gen_unbind) output_unbind(out, conv, realname);
    output_default(out, realname);
    if(gen_copy) output_copy(out, conv, realname);
    if(gen_assign) output_assign(out, conv, realname);
    output_footer(out);
  }
}


int main(int argc, char *argv[])
{
  vector<member> ms;
  if(argc != 3)
  {
    cout << argv[0] << " input.bsonbind output.hpp" << endl;
    return 1;
  }
  
  {
    ifstream in(argv[1]);
    if(!in.is_open())
    {
      cerr << "Failed to open " << argv[1] << " for reading" << endl;
      return 1;
    }
    process_file(in, ms);
    in.close();
  }
  
  {
    ofstream out(argv[2]);
    if(!out.is_open())
    {
      cerr << "Failed to open " << argv[2] << " for writing" << endl;
      return 1;
    }
    output_file(out, ms, argv[2]);
    out.close();
  }
  
  return 0;
}