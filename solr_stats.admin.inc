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

  $info['key'] = $group_type;
  $output = dd_get_form('solr_stats_query_form', $info);

  dd_set_breadcrumb(array(
    l(t('solr_stats', '数据统计'), 'admin/solr/stats'),
    $info['title']
  ));
 
  return $output;
}
// }}}

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
  if (!$info = solr_get_search_group($group_type)) {
    echo '';
    exit;
  }

  $solr = new solr($group_type);

  if (!empty($_POST)) {
    $v = $_POST;
    $is_export = false;
   
    $query = array();

    $args = array();

    if (!empty($v['query']['q'])) {
      $args[] = $v['query']['q'];
      unset($v['query']['q']);
    }

    if (!empty($v['query'])) {
      foreach ($v['query'] as $key => $value) {
        if (!empty($value)) {
          $args[] = $key . ':' . $value;
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

    module_alter_all('solr_stats_query_submit', $info, $query, $v);
    $result = $solr->query($query);
  } else if (!empty($_GET['export'])) {
    $query = dd_decrypt($_GET['data']);
    $result = $solr->query($query, 'GET', false);
    $is_export = true;
  }

  $values = array();

  $is_export_text = false;

  // 主要结果
  if (!empty($result->response) && !empty($result->response->numFound)) {
    $values[] = t('solr_stats', '默认统计');
    $values[] = array(t('solr_stats', '总记录'), $result->response->numFound);
    $values[] = '';
    //foreach ($result->response->docs as $doc) {
    //}
  }
 
  // stats 结果
  if (!empty($result->stats)) {
    foreach ($result->stats as $key => $r) {
      $r = (array) $r;
      if (empty($r)) continue;
      switch ($key) {
        case 'stats_fields':
          foreach ($r as $field_key => $d) {
            $label = solr_stats_get_field_label($info, $field_key);
            $values[] = t('solr_stats', '按!key汇总统计', array('!key' => $label));
            $values[] = array($label, t('solr_stats', 'Value'));
            foreach ($d as $_k => $_v) {
              $values[] = array(solr_stats_get_stats_label($_k), $_v);
            }
            $values[] = '';
          }
        break;
      }
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
      }
    }
    //if (!empty($query['facet.pivot']) && 
    //$result->facet_counts->facet_pivot->{$query['facet.pivot']}) {
    //  foreach ($result->facet_counts->facet_pivot->{$query['facet.pivot']} as $_o) {
    //    $data['facet']['count'] += count($_o->pivot);
    //  }
    //} else {
    //  foreach ($result->facet_counts->facet_fields->{$query['facet.field']} as $_k => $o) {
    //    $data['facet']['count']++;
    //    if ($data['facet']['count'] < 2000 && empty($v['export'])) {
    //      $data['facet']['items'][$_k] = $o;
    //    }
    //  }
    //}
  }

  // group 结果
  if (!empty($result->grouped)) {

  }
 
  if (!empty($is_export)) {
    solr_stats_export_csv(array('title' => $group_type, 'rows' => $values));
    exit;
  }

  if (!empty($values)) {
    $rows_count = 1;
    foreach ($values as $data) {
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
    }
    
    $url = url('admin/solr/stats/' . $group_type . '/ajax', array(
      'query' => array(
        'export' => 1,
        'data' => dd_encrypt($solr->queryString) 
      ),
    ));

    $output = '<div class="solr_stats_view_export_link">';
    $output .= '<a href="' . $url . '" target="_blank">';
    $output .= t('solr_stats', '导出数据');
    $output .= '</a>';
    if (!empty($is_export_text)) {
      $output .= t('solr_stats', '(匹配的数据过多，若需查看全部，请导出)');
    }
    $output .= '</div>';
    $output .= str_replace('[rows]', $rows_count, theme('table', array(), $table));
    echo '$("#solr_stats_query_form_body_wrapper").html("' . addslashes(str_replace("\n", '', $output)) . '")';
  }

  exit;
}
// }}}

