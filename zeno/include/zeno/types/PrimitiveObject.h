#pragma once

#include <zeno/core/IObject.h>
#include <zeno/types/AttributeArray.h>
#include <zeno/utils/vec.h>
#include <variant>
#include <memory>
#include <string>
#include <vector>
#include <map>

namespace zeno {

struct PrimitiveObject : IObjectClone<PrimitiveObject> {

  // vertex attributes (including "pos"):
  std::map<std::string, AttributeArray> m_attrs;
  size_t m_size{0};

  // todo: legacy topology storage, deprecate:
  std::vector<int> points;
  std::vector<vec2i> lines;
  std::vector<vec3i> tris;
  std::vector<vec4i> quads;

  // new topology storage that supports n polygons:
  std::vector<int> loops;
  std::vector<std::pair<int, int>> polys;

#ifndef ZENO_APIFREE
  ZENO_API virtual void dumpfile(std::string const &path) override;
#else
  virtual void dumpfile(std::string const &path) override {}
#endif

  template <class T> std::vector<T> &add_attr(std::string const &name) {
    if (!has_attr(name))
      m_attrs[name] = std::vector<T>(m_size);
    return attr<T>(name);
  }

  template <class T> std::vector<T> &add_attr(std::string const &name, T value) {
    if (!has_attr(name))
      m_attrs[name] = std::vector<T>(m_size, value);
    return attr<T>(name);
  }

  AttributeArray &attr(std::string const &name) { return m_attrs.at(name); }

  template <class T> std::vector<T> &attr(std::string const &name) {
    return std::get<std::vector<T>>(m_attrs.at(name));
  }

  template <class T> std::vector<T> const &attr(std::string const &name) const {
    return std::get<std::vector<T>>(m_attrs.at(name));
  }

  AttributeArray const &attr(std::string const &name) const {
    return m_attrs.at(name);
  }

  bool has_attr(std::string const &name) const {
    return m_attrs.find(name) != m_attrs.end();
  }

  template <class T> bool attr_is(std::string const &name) const {
    return std::holds_alternative<std::vector<T>>(m_attrs.at(name));
  }

  size_t size() const { return m_size; }

  void resize(size_t size) {
    m_size = size;
    for (auto &[key, val] : m_attrs) {
      std::visit([&](auto &val) { val.resize(m_size); }, val);
    }
  }
};

} // namespace zeno
