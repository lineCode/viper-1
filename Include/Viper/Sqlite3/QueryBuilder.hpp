#ifndef VIPER_SQLITE3_QUERY_BUILDER_HPP
#define VIPER_SQLITE3_QUERY_BUILDER_HPP
#include <string>
#include "Viper/CreateTableStatement.hpp"
#include "Viper/DeleteStatement.hpp"
#include "Viper/InsertRangeStatement.hpp"
#include "Viper/SelectClause.hpp"
#include "Viper/SelectStatement.hpp"
#include "Viper/UpdateStatement.hpp"
#include "Viper/UpsertStatement.hpp"
#include "Viper/Sqlite3/DataTypeName.hpp"

namespace Viper::Sqlite3 {
namespace Details {
  template<typename B, typename E, typename F>
  void append_list(B b, E e, std::string& query, F&& f) {
    auto prepend_comma = false;
    auto size = query.size();
    for(auto i = b; i != e; ++i) {
      if(prepend_comma) {
        query += ',';
      }
      f(*i, query);
      if(!prepend_comma) {
        prepend_comma = size != query.size();
      }
    }
  }

  template<typename T, typename F>
  void append_list(const T& list, std::string& query, F&& f) {
    append_list(list.begin(), list.end(), query, f);
  }

  template<typename T>
  void append_list(const T& list, std::string& query) {
    return append_list(list, query,
      [] (auto& item, std::string& query) {
        query += item;
      });
  }
}

  //! Builds a create table query statement.
  /*!
    \param statement The statement to build.
    \param query The string to store the query in.
  */
  template<typename T>
  void build_query(const CreateTableStatement<T>& statement,
      std::string& query) {
    query += "BEGIN;CREATE TABLE ";
    if(statement.get_exists_flag()) {
      query += "IF NOT EXISTS ";
    }
    query += statement.get_name() + '(';
    Details::append_list(statement.get_row().get_columns(), query,
      [] (auto& column, auto& query) {
        query += column.m_name + ' ' + get_name(*column.m_type);
        if(!column.m_is_nullable) {
          query += " NOT NULL";
        }
      });
    if(!statement.get_row().get_indexes().empty() &&
        statement.get_row().get_indexes().front().m_is_primary) {
      query += ",PRIMARY KEY(";
      Details::append_list(statement.get_row().get_indexes().front().m_columns,
        query);
      query += ')';
    }
    query += ");";
    for(auto& current_index : statement.get_row().get_indexes()) {
      if(current_index.m_is_primary) {
        continue;
      }
      if(current_index.m_is_unique) {
        query += "CREATE UNIQUE INDEX";
      } else {
        query += "CREATE INDEX";
      }
      query += " IF NOT EXISTS " + statement.get_name() + "_" +
        current_index.m_name + " ON " + statement.get_name() + "(";
      Details::append_list(current_index.m_columns, query);
      query += ");";
    }
    query += "COMMIT;";
  }

  //! Builds a delete query statement.
  /*!
    \param statement The statement to build.
    \param query The string to store the query in.
  */
  inline void build_query(const DeleteStatement& statement,
      std::string& query) {
    query += "DELETE FROM " + statement.get_table();
    if(statement.get_where().has_value()) {
      query += " WHERE ";
      statement.get_where()->append_query(query);
    }
    query += ';';
  }

  //! Builds an insert range query statement.
  /*!
    \param statement The statement to build.
    \param query The string to store the query in.
  */
  template<typename T, typename B, typename E>
  void build_query(const InsertRangeStatement<T, B, E>& statement,
      std::string& query) {
    if(statement.get_begin() == statement.get_end() ||
        statement.get_row().get_columns().empty()) {
      return;
    }
    query += "INSERT INTO ";
    query += statement.get_table();
    query += " (";
    Details::append_list(statement.get_row().get_columns(), query,
      [] (auto& column, auto& query) {
        query += column.m_name;
      });
    query += ") VALUES ";
    Details::append_list(statement.get_begin(), statement.get_end(), query,
      [&] (auto& column, auto& query) {
        query += '(';
        auto prepend_comma = false;
        for(auto i = 0; i <
            static_cast<int>(statement.get_row().get_columns().size()); ++i) {
          if(prepend_comma) {
            query += ',';
          }
          prepend_comma = true;
          statement.get_row().append_value(column, i, query);
        }
        query += ')';
      });
    query += ';';
  }

  //! Builds an update statement.
  /*!
    \param statement The statement to build.
    \param query The string to store the query in.
  */
  inline void build_query(const UpdateStatement& statement,
      std::string& query) {
    query += "UPDATE ";
    query += statement.get_table();
    query += " SET ";
    query += statement.get_set().m_column;
    query += " = ";
    statement.get_set().m_value.append_query(query);
    if(statement.get_where()) {
      query += " WHERE ";
      statement.get_where()->append_query(query);
    }
    query += ';';
  }

  //! Builds an upsert query statement.
  /*!
    \param statement The statement to build.
    \param query The string to store the query in.
  */
  template<typename T, typename B, typename E>
  void build_query(const UpsertStatement<T, B, E>& statement,
      std::string& query) {
    if(statement.get_begin() == statement.get_end() ||
        statement.get_row().get_columns().empty()) {
      return;
    }
    query += "INSERT INTO ";
    query += statement.get_table();
    query += " (";
    Details::append_list(statement.get_row().get_columns(), query,
      [] (auto& column, auto& query) {
        query += column.m_name;
      });
    query += ") VALUES ";
    Details::append_list(statement.get_begin(), statement.get_end(), query,
      [&] (auto& column, auto& query) {
        query += '(';
        auto prepend_comma = false;
        for(auto i = 0; i <
            static_cast<int>(statement.get_row().get_columns().size()); ++i) {
          if(prepend_comma) {
            query += ',';
          }
          prepend_comma = true;
          statement.get_row().append_value(column, i, query);
        }
        query += ')';
      });
    query += " ON CONFLICT(";
    auto indicies = std::vector<std::string>();
    for(auto& index : statement.get_row().get_indexes()) {
      if(index.m_is_unique) {
        indicies.insert(indicies.end(), index.m_columns.begin(),
          index.m_columns.end());
      }
    }
    Details::append_list(indicies, query);
    query += ") DO UPDATE SET ";
    Details::append_list(statement.get_row().get_columns(), query,
      [&] (auto& column, auto& query) {
        auto is_unique = std::find(indicies.begin(), indicies.end(),
          column.m_name) != indicies.end();
        if(!is_unique) {
          query += column.m_name;
          query += " = ";
          query += "excluded." + column.m_name;
        }
      });
    query += ';';
  }

  //! Builds a select query clause.
  /*!
    \param clause The clause to build.
    \param query The string to store the query in.
  */
  inline void build_query(const SelectClause& clause, std::string& query) {
    query += "SELECT ";
    Details::append_list(clause.get_columns(), query);
    query += " FROM ";
    auto& from = clause.get_from();
    if(auto t = std::get_if<std::string>(&from)) {
      query += *t;
    } else if(auto t = std::get_if<std::shared_ptr<SelectClause>>(&from)) {
      query += '(';
      build_query(**t, query);
      query += ") AS alias";
    }
    if(clause.get_where() != std::nullopt) {
      query += " WHERE ";
      clause.get_where()->append_query(query);
    }
    if(clause.get_order() != std::nullopt &&
        !clause.get_order()->m_columns.empty()) {
      query += " ORDER BY ";
      if(clause.get_order()->m_columns.size() == 1) {
        query += clause.get_order()->m_columns.front();
      } else {
        query += '(';
        Details::append_list(clause.get_order()->m_columns, query);
        query += ')';
      }
      if(clause.get_order()->m_order == Order::ASC) {
        query += " ASC";
      } else {
        query += " DESC";
      }
    }
    if(clause.get_limit() != std::nullopt) {
      query += " LIMIT ";
      query += std::to_string(clause.get_limit()->m_value);
    }
  }

  //! Builds a select query statement.
  /*!
    \param statement The statement to build.
    \param query The string to store the query in.
  */
  template<typename T, typename D>
  void build_query(const SelectStatement<T, D>& statement,
      std::string& query) {
    build_query(statement.get_clause(), query);
    query += ';';
  }
}

#endif
