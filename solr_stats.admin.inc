<?php
// $Id$

// public solr_stats_admin() {{{ 
/**
 * solr_stats_admin
 *  当前用户有权限访问的所有统计分组
 * 
 * @access public
 * @return string
 */
function solr_stats_admin() {
  $output = '';

  $items = array();
  foreach (solr_stats_get_group() as $key => $info) {
    if (user_access('solr stats admin ' . $key . ' view')) {
      $items[] = l($info['title'], 'admin/solr/stats/' . $key);
    }
  }

  $output = theme('item_list', $items, t('solr_stats', '请选择统计项目'));

  dd_set_breadcrumb(array(
    t('solr_stats', '数据统计')
  ));
 
  return $output;
}
// }}}

// public solr_stats_admin_stats($group_type) {{{ 
/**
 * solr_stats_admin_stats
 *  数据统计
 * 
 * @param string $group_type 
 *  分组名称
 * @access public
 * @return string
 */
function solr_stats_admin_stats($group_type) {
  if (!$info = solr_get_search_group($group_type)) {
    dd_get_not();
  }

  $tabs = array();
  foreach (solr_stats_get_group() as $key => $i) {
    if (user_access('solr stats admin ' . $key . ' view')) {
      $tabs[] = l($i['title'], 'admin/solr/stats/' . $key);
    }
  }

  dd_set_tabs($tabs);
 
  $info['key'] = $group_type;
  $output = dd_get_form('solr_stats_query_form', $info);

  dd_set_breadcrumb(array(
    l(t('solr_stats', '数据统计'), 'admin/solr/stats'),
    $info['title']
  ));
 
  return $output;
}
// }}}

function solr_stats_admin_stats_ajax_text_filter($value, &$query = array()) {
  foreach ($value as $text) {
    if (strpos($text, '=') !== false) {
      $_a = explode('=', $text, 2);
      if (strpos($_a[0], '[]') !== false) {
        $query[rtrim($_a[0], '[]')][] = $_a[1];
      } else {
        $query[$_a[0]] = $_a[1];
      }
    }
  }
}

function solr_stats_admin_stats_ajax_batch_filter($value, $solr) {
  if ($batch = dd_line_to_array($value)) {
    $query = array(); 
    foreach ($batch as $v) {
      if (preg_match('/\-\-\-(.*?)\-\-\-/i', $v, $mat)) {
        $result = $solr->query(array(
          'q' => '*:*',
          'facet' => 'on',
          'facet.field' => $mat[1],
          'facet.mincount' => 1,
          'facet.limit' => 1000
        ));
        if (!empty($result->facet_counts) && !empty($result->facet_counts->facet_fields) && !empty($result->facet_counts->facet_fields->{$mat[1]})) {
          foreach ($result->facet_counts->facet_fields->{$mat[1]} as $key => $r) {
            $query[] = str_replace('---' . $mat[1] . '---',  $key, $v);
          }
        }
      } else {
        $query[] = $v;
      }
    }
    return $query;
  }
}

// public solr_stats_admin_stats_ajax($group_type) {{{ 
/**
 * solr_stats_admin_stats_ajax
 *  查询处理
 * 
 * @param string $group_type 
 * @access public
 * @return void
 */
function solr_stats_admin_stats_ajax($group_type) {
  $solr = new solr($group_type);
  $output = '';
  if (!empty($_POST)) {
    $v = $_POST;
    $values = array();

    if (!empty($v['batch']) && $batch = solr_stats_admin_stats_ajax_batch_filter($v['batch'], $solr)) {
      if (!empty($v['query']['q'])) {
        $old_query = ' AND ' . $v['query']['q'];
      } else {
        $old_query = '';
      }
      $last_key = '';
      foreach ($batch as $_v) {
        $v['query']['q'] = $_v . $old_query;
        $last_key = $_v;
        $rows = _solr_stats_admin_stats_ajax($group_type, $solr, $v);
        $values[] = t('solr_stats', '查询条件：!query', array(
          '!query' => urldecode($solr->queryData['q'])
        ));
        $values[] = t('solr_stats', '完整参数：!query', array(
          '!query' => urldecode($solr->queryString)
        ));
        $values = array_merge($values, $rows);
        $values[] = '';
      }
      $solr->queryString .= '&batch=' . urlencode($v['batch']);
      $solr->queryString = str_replace('q=' . urlencode($last_key), '---batch---', $solr->queryString);
      //$values[] = $solr->queryString;
    } else {
      $values = _solr_stats_admin_stats_ajax($group_type, $solr, $v);
      array_unshift($values, t('solr_stats', '完整参数：!query', array(
        '!query' => urldecode($solr->queryString)
      )));
      array_unshift($values, t('solr_stats', '查询条件：!query', array(
        '!query' => urldecode($solr->queryData['q'])
      )));
    }
 
    $i = 0;
    $rows_count = 1;
    foreach ($values as $data) {
      // 最多只显示 500 行，以免页面过大
      if ($i > 500) {
        $table[][] = array(
          'data' => t('solr_stats', '匹配的数据过多，若需查看全部，请导出'),
          'colspan' => '[rows]'
        );
        break;
      }
      if (is_array($data)) {
        $c = count($data);
        if ($c > $rows_count) $rows_count = $c;
        $table[] = $data;
      } else {
        $rows = array('data' => $data, 'colspan' => '[rows]');
        if (empty($data)) {
          $rows['class'] = 'solr_stats_view_rows_line';
        } else {
          $rows['class'] = 'solr_stats_view_rows_title';
        }
        $table[][] = $rows;
      }
      $i++;
    }
    
    $url = url('admin/solr/stats/' . $group_type . '/ajax', array(
      'query' => array(
        'export' => 1,
        'data' => dd_encrypt($solr->queryString) 
      ),
    ));

    if (count($values) > 20) {
      $output = '<div class="solr_stats_view_export_link">';
      $output .= '<a href="' . $url . '" target="_blank">';
      $output .= t('solr_stats', '导出数据');
      $output .= '</a>';
      if (!empty($solr->is_export_text) || !empty($v['batch'])) {
        $output .= t('solr_stats', '(匹配的数据过多，若需查看全部，请导出)');
      }
      $output .= '</div>';
    }
    $output .= str_replace('[rows]', $rows_count, theme('table', array(), $table));
    echo '$("#solr_stats_query_form_body_wrapper").html("' . addslashes(str_replace("\n", '', $output)) . '");';
  } else if (!empty($_GET['export'])) {
    $v = dd_decrypt($_GET['data']);
    $query = array();
    parse_str(urldecode($v), $query);
    if (!empty($query['batch']) && $batch = solr_stats_admin_stats_ajax_batch_filter(urldecode($query['batch']), $solr)) {
      $values = array();
      foreach ($batch as $key) {
        if ($rows = _solr_stats_admin_stats_ajax($group_type, $solr,str_replace('---batch---', 'q=' . urlencode($key), $v) , 1)) {
          $values[] = t('solr_stats', '查询条件：!query', array(
            '!query' => urldecode($solr->queryData['q'])
          ));
          $values = array_merge($values, $rows);
          $values[] = '';
        }
      }
      solr_stats_export_csv(array('title' => $group_type, 'rows' => $values));
    } else if ($values = _solr_stats_admin_stats_ajax($group_type, $solr, $v, 1)) {
      solr_stats_export_csv(array('title' => $group_type, 'rows' => $values));
    }
  }

  exit;
}

function _solr_stats_admin_stats_ajax($group_type, $solr, $v, $is_export = false) {
  if (!$info = solr_get_search_group($group_type)) {
    echo '';
    exit;
  }
 
  if (empty($is_export)) {
    $query = array();

    $args = array();

    if (!empty($v['query']['q'])) {
      $args[] = $v['query']['q'];
      unset($v['query']['q']);
    }

    if (!empty($v['query'])) {
      foreach ($v['query'] as $key => $value) {
        // 自处理参数，不再解析
        if (substr($key, 0, 8) == '__custom') continue;

        if (!empty($value)) {
          if ($key == '_params') {
            if (is_array($value)) {
              foreach ($value as $_v) {
                $args[] = $_v;
              }
            }
            continue;
          }
          if (!is_array($value)) {
            $args[] = $key . ':' . $value;
          } else {
            $args[] = $key . ':(' . implode(' OR ', $value) . ')';
          }
        }
      }

      if (!empty($v['query']['sid'])) {
        $solr->custom_data['sid'] = $v['query']['sid'];
      }
    }

    if (!empty($args)) {
      $query['q'] = implode(' AND ', $args);
    } else {
      $query['q'] = '*:*';
    }
    
    if (!empty($v['param']['default']) && $value = dd_line_to_array($v['param']['default'], 1)) {
      solr_stats_admin_stats_ajax_text_filter($value, $query);
      unset($v['param']['default']);
    }

    if (!empty($v['param'])) {
      foreach ($v['param'] as $key => $value) {
        if (!empty($value)) {
          $query[$key] = $value;
        }
      }
    }

    if (!isset($query['rows'])) {
      $query['rows'] = 10;
    }

    if (empty($query['fl'])) {
      $query['fl'] = '*';
    }

    if (!empty($query['stats.field'])) {
      $query['stats'] = 'on';
    }

    if (!empty($query['facet.field'])) {
      $query['facet'] = 'on';
    }

    if (!empty($query['facet'])) {
      if (empty($query['facet.limit'])) {
        $query['facet.limit'] = 500000;
      }
      if (empty($query['facet.mincount'])) {
        $query['facet.mincount'] = 1;
      }
    }

    if (!empty($query['group.field'])) {
      $query['group'] = 'on';
      $query['group.ngroups'] = 'on';
    }

    module_alter_all('solr_stats_query_submit', $info, $query, $v);
    $result = $solr->query($query);
  } else {
    $result = $solr->query($v, 'GET', false);
  }

  $values = array();

  $is_export_text = false;

  // 主要结果
  if (!empty($result->response) && !empty($result->response->numFound)) {
    $values[] = t('solr_stats', '匹配总记录(!count)', array(
      '!count' => $result->response->numFound
    ));
    $values[] = '';
  }

  // stats 结果
  if (!empty($result->stats)) {
    $rows = array();
    foreach ($result->stats as $key => $r) {
      $r = (array) $r;
      if (empty($r)) continue;
      switch ($key) {
        case 'stats_fields':
          foreach ($r as $field_key => $d) {
            $d = (array) $d;
            if (empty($d)) continue;
            $__k = $key . $field_key;
            $label = solr_stats_get_field_label($info, $field_key);
            $rows[$__k] = array('_all' => $label . '(' . $field_key . ')');
            foreach ($d as $_k => $_v) {
              if ($_k != 'facets') {
                $rows[$__k][$_k] = $_v;
              } else if (!empty($_v)) {
                foreach ($_v as $_k2 => $_v2) {
                  $_label = solr_stats_get_field_label($info, $_k2);
                  $c = 0;
                  foreach ($_v2 as $_k3 => $_v3) {
                    $rows[$__k . $_k2 . $_k3] = array('_all' => $label . '|' . $_label . '|' . $_k3);
                    if (!empty($is_export) || $c < 100) {
                      foreach ($_v3 as $_k4 => $_v4) {
                        if ($_k4 != 'facets') {
                          $rows[$__k . $_k2 . $_k3][$_k4] = $_v4;
                        }
                      }
                    } else {
                      unset($rows[$__k . $_k2 . $_k3]);
                      $is_export_text = true;
                    }
                    $c++;
                  }
                $rows[] = '';
                }
              }
            }
          }
        break;
      }
    }

    if (!empty($rows)) {
      $values[] = '汇总统计';
      $stats_header = solr_stats_get_stats_label('_all');
      $values[] = array('_all' => '字段')+$stats_header;
      foreach ($rows as $r) {
        $values[] = $r;
      }
      $values[] = '';
    }
  }

  // facet 结果
  if (!empty($result->facet_counts)) {
    foreach ($result->facet_counts as $key => $r) {
      $r = (array) $r;
      if (empty($r)) continue;
      switch ($key) {
        case 'facet_fields':
          foreach ($r as $field_key => $d) {
            $label = solr_stats_get_field_label($info, $field_key);
            $values[] = t('solr_stats', '按!key维度统计', array('!key' => $label));
            $values[] = array($label, t('solr_stats', 'Value'));
            $c = 0;
            foreach ($d as $_k => $_v) {
              if (!empty($is_export) || $c < 100) {
                $values[] = array($_k, $_v);
              } else {
                $is_export_text = true;
              }
              $c++;
            }
            $values[] = array(t('solr_stats', '!key总数', array('!key' => $label)), $c);
            $values[] = '';
          }
        break;
        case 'facet_queries':
          $values[] = t('solr_stats', '按查询维度统计');
          $values[] = array(t('solr_stats', '查询参数'), t('solr_stats', '匹配记录'));
          foreach ($r as $_k => $_v) {
            $values[] = array($_k, $_v);
          }
          $values[] = '';
        break;
        case 'facet_pivot':
          foreach ($r as $field_key => $vd) {
            if (strpos($field_key, ',') !== false) {
              $label = ' ';
              foreach (explode(',', $field_key) as $__k) {
                $label .= solr_stats_get_field_label($info, $__k) . ' ';
              }
            } else {
              $label = solr_stats_get_field_label($info, $field_key);
            }
            $values[] = t('solr_stats', '按!key维度统计', array('!key' => $label));
            foreach ($vd as $d) {
              if (empty($d->pivot)) continue;
              $values[] = array(solr_stats_get_field_label($info, $d->field) . '：' . $d->value, $d->count);
              $c = 0;
              foreach ($d->pivot as $_k => $_v) {
                if (!empty($is_export) || $c < 10) {
                  $values[] = array($_v->value, $_v->count);
                } else {
                  $is_export_text = true;
                }
                $c++;
              }
              $values[] = '';
              $values[] = array(t('solr_stats', '!key总数', array('!key' => $label)), $c);
              $values[] = '';
            }
          }
        break;
      }
    }
  }

  // group 结果
  if (!empty($result->grouped)) {
    foreach ($result->grouped as $key => $r) {
      $label = solr_stats_get_field_label($info, $key);
      $values[] = t('solr_stats', '按!key排重统计', array('!key' => $label));
      $values[] = array(t('solr_stats', '排重前'), $r->matches);
      $values[] = array(t('solr_stats', '排重后'), $r->ngroups);
    }
  }

  // 主要结果
  if (!empty($result->response) && !empty($result->response->numFound)) {
    if (!empty($result->response->docs)) {
      $values[] = t('solr_stats', '明细示例：');
      $doc = $result->response->docs[0];
      $header = array();
      foreach ($doc as $key => $i) {
        $header[$key] = solr_stats_get_field_label($info, $key);
      }
      $values[] = $header;
      foreach ($result->response->docs as $doc) {
        $rows = array();
        foreach ($header as $key => $i) {
          if (!empty($doc->{$key})) {
            $rows[] = is_array($doc->{$key}) ? implode(',', $doc->{$key}) : $doc->{$key};
          } else {
            $rows[] = '';
          }
        }
        $values[] = $rows;
      }
    }
    $values[] = '';
  }
 
  $solr->is_export_text = $is_export_text;

  return $values;
    //echo '$("#solr_stats_query_form_type_query_q").val("' . urldecode($solr->queryData['q']) . '");';
}
// }}}

