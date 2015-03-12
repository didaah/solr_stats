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

  $v = $_POST;

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
        $query[$_a[0]] = $_a[1];
      }
    }
    unset($v['param']['default']);
  }

  if (!empty($v['param'])) {
    foreach ($v['param'] as $key => $value) {
      $query[$key] = $value;
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

   $test = array(
    '个人' => array(
      'q' => 'param_t:是',
      'rows' => 1,
      'facet' => 'on',
      'facet.limit' => 10000,
      'facet.mincount' => 1,
      //'facet.pivot' => 'param_r,param_m',
      'facet.field' => 'param_m'
    ),   
  );

  $city = array(
    '全国' => '',
    '一线城市' => '(北京 OR 上海 OR 广州 OR 深圳)',
    '二线城市' => '(杭州 OR 南京  OR 济南  OR 重庆  OR 青岛  OR 大连  OR 宁波  OR 厦门  OR 成都  OR 武汉  OR 哈尔滨  OR 西安  OR 沈阳  OR 长春  OR 长沙  OR 福州  OR 郑州  OR 石家庄  OR 苏州  OR 佛山  OR 东莞  OR 无锡  OR 烟台  OR 太原  OR 合肥  OR 南昌  OR 南宁  OR 昆明  OR 温州  OR 淄博  OR 唐山)',
  );

  $result = $solr->query($query);

  $data = array(
    'count' => 0,
    'items' => array(),
    'facet' => array(
      'count' => 0,
      'items' => array(),
    ),
    'group' => array(
      'count' => 0,
      'items' => array(),
    ),
    'stats' => array(
      'count' => 0,
      'items' => array(),
    ),
  );

  // 主要结果
  if (!empty($result->response) && !empty($result->response->numFound)) {
    $data['count'] = $result->response->numFound;
    foreach ($result->response->docs as $doc) {
      $data['items'][] = $doc;
    }
  }

  // facet 结果
  if (!empty($result->facet_counts) && !empty($result->facet_counts->facet_fields->{$query['facet.field']})) {
    if (!empty($query['facet.pivot']) && 
    $result->facet_counts->facet_pivot->{$query['facet.pivot']}) {
      foreach ($result->facet_counts->facet_pivot->{$query['facet.pivot']} as $_o) {
        $data['facet']['count'] += count($_o->pivot);
      }
    } else {
      foreach ($result->facet_counts->facet_fields->{$query['facet.field']} as $_k => $o) {
        $data['facet']['count']++;
        if ($data['facet']['count'] < 2000 && empty($v['export'])) {
          $data['facet']['items'][$_k] = $o;
        }
      }
    }
  }

  // group 结果
  if (!empty($result->grouped)) {

  }
  
  // stats 结果
  if (!empty($result->stats) && !empty($result->stats->stats_fields->{$query['stats.field']})) {
    $data['stats']['count'] = 1;
    $data['stats']['items'] = $result->stats->stats_fields->{$query['stats.field']};
  }

  $output = '';

  if (!empty($data['count'])) {
    $output .= '<h2>默认统计</h2>';
    $output .= '<p>匹配的总记录：' . $data['count'] . '</p>';
  }
 
  if (!empty($data['stats']['count'])) {
    $output .= '<h2>按 ' . $query['stats.field'] . ' 汇总统计</h2>';
    $header = array();
    $rows = array();
    foreach ($data['stats']['items'] as $_k => $_v) {
      if (!is_array($_v) && !is_object($_v)) {
        $header[] = $_k;
        $rows[] = $_v;
      }
    }
    $output .= theme('table', $header, array($rows), array('class' => 'center_table'));
  }
  
  if (!empty($data['facet']['count'])) {
    $output .= '<h2>按 ' . $query['facet.field'] . ' 统计</h2>';
    $output .= '<p>' . $query['facet.field'] . '数量：' . $data['facet']['count'] . '</p>';
    if (!empty($data['facet']['items'])) {
      $table = array();
      $header = array(t('solr_stats', '名称'), t('solr_stats', '数量'));
      foreach ($data['facet']['items'] as $_k => $_v) {
        $table[] = array($_k, $_v);
      }
      $output .= theme('table', $header, $table, array('class' => 'center_table'));
    }
  }

  echo '$("#solr_stats_query_form_body_wrapper").html("' . addslashes(str_replace("\n", '', $output)) . '")';

  //print_r($data);

  exit;
}
// }}}

