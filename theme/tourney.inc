<?php

/*
 * @file
 * Preprocess functions for all theme implementations provided by tourney.
 */
 
/**
 * 
 */
function template_preprocess_tourney_tournament_render(&$vars) {
  $plugin = &$vars['plugin'];
  
  // Initialize variables exposed in template.
  $vars['header'] = $vars['footer'] = $vars['matches'] = '';
  
  drupal_add_css(drupal_get_path('module', 'tourney') . '/theme/tourney.css');
  // Let the plugin preprocess variables
  $plugin->preprocess('tourney-tournament-render', $vars);
  
  // Build the tourney container classes.
  $vars['classes'] = join(' ', $vars['classes_array']);
}

/**
 * @todo: Think about whether or not I can move some of this logic to 
 *   TourneyController::preprocess()
 */
function template_preprocess_tourney_match_render(&$vars) {
  $plugin = &$vars['plugin'];
  $tournament = $plugin->tournament;

  $match = $vars['match'];
  $matchInfo = $match;
  if (!is_object($match)) {
    if (is_object($plugin->tournament)) {
      $match = $plugin->tournament->getMatchById($match['id']);
    }
  }
  if (is_a($match, 'TourneyMatchEntity')) {
    $match->getMatchInfo();
    $matchInfo = $match->matchInfo;
  }

  $editable = FALSE;

  // Let the plugin preprocess variables
  $plugin->preprocess('tourney-match-render', $vars);
  
  // If we have a tournament object, load the match from tournament.
  
  // Get the contestants for the match, if match object exists.
  $contestants = !empty($match) && is_object($match) ? $match->getMatchContestants() : array();
  
  // If there is one contestants, pad the beginning if it's occupying slot 2
  if (count($contestants) == 1) {
    $first = current($contestants);
    if ($first->slot == 2) {
      // Fill in a NULL contestant in position 1.
      $contestants[1] = NULL;
      $contestants = array_reverse($contestants, TRUE);
      $editable = TRUE;
    }
  }
  // Fill in null contestants if two contestants are not assigned to match.
  $i = count($contestants);
  while (count($contestants) < 2) {
    $contestants[++$i] = (object) array('name' => NULL, 'id' => NULL);
    // This match is editable when at least one of the contestants is NULL.
    $editable = TRUE;
  }
  
  // Get the contestant output from the contestant template
  $contestant_output = '';
  $seeds = isset($matchInfo['seeds']) ? $matchInfo['seeds'] : '';

  //find the order of how the contestants to games are stored
  foreach($match->contestantIds as $key => $value) {
    $contestant_map[$value['eid']] = $key;
  }

  foreach (array_values($contestants) as $slot => $contestant) {
    $slot++;
    $seed_name = '';

    if (!is_object($contestant)) {
      $contestant = (object) $contestant;
    }
    if (isset($seeds[$slot]) && is_object($tournament) && get_class($tournament) == 'TourneyTournamentEntity') {
      if ($tournament->get('show_seed_names', FALSE)) {
        $seed_name = $tournament->get('seed_' . $seeds[$slot], $seeds[$slot]);
      }
      else {
        $seed_name = $seeds[$slot];
      }
    }
    if(is_numeric($contestant_map[$contestant->eid])) {
      $wins = (isset($match->matchInfo['game_wins'][$contestant_map[$contestant->eid]])) ? $match->matchInfo['game_wins'][$contestant_map[$contestant->eid]] : 0;
    } else {
      $wins = 0;
    }
    $contestant->slot = !empty($contestant->slot) ? $contestant->slot : $slot;
    $contestant_output .= theme('tourney_contestant', array(
      'contestant' => $contestant, 
      'seed' => $seed_name,
      'plugin' => $plugin,
      'winner' => property_exists($contestant, 'eid') && $contestant->eid == $match->winner ? $match->winner : FALSE,
      'winner_set' => !empty($match->winner),
      'wins' => $wins,
    ));
  }
  
  // If we have a tournament object, get the match url from the plugin.
  if (is_object($tournament) && get_class($tournament) == 'TourneyTournamentEntity') {
    $match_uri = $plugin->getMatchURI($matchInfo['id']);
    if ($match_uri && !$editable || user_access('edit tourney')) {
      $editable = $editable == TRUE && $plugin->canSetContestants($matchInfo['id']) && user_access('edit tourney');
      $contestant_output = l($contestant_output, $match_uri . ($editable ? "/edit" : ''), array(
        'html' => TRUE,
        'attributes' => array('class' => 'match-link'),
        'query' => $editable ? array('destination' => $match_uri) : array(),
      ));
    }
  }

  $vars['complete'] = isset($match) && is_object($match) && $match->winner !== '';
  $vars['is_bye'] = isset($matchInfo['bye']) && $matchInfo['bye'] == TRUE;
  $vars['hide'] = array_key_exists('hide', $matchInfo) && $matchInfo['hide'] == TRUE;

  $vars['match_classes_array'] = array(
    'round-' . $matchInfo['round'],
    'bracket-' . $matchInfo['bracket'] . '-round-' . $matchInfo['round']
  );

  if ($editable) $vars['match_classes_array'][] = 'no-teams-set';
  if ($vars['complete']) $vars['match_classes_array'][] = 'complete';
  if ($vars['is_bye']) $vars['match_classes_array'][] = 'bye';
  if ($vars['hide']) $vars['match_classes_array'][] = 'hide-match';
  if (isset($match) && is_object($match) && $match->status == 0) $vars['match_classes_array'][] = 'not-necessary';
  $vars['match_classes'] = implode(' ', $vars['match_classes_array']);

  $vars['contestants'] = $contestant_output;
  $vars['has_children'] = isset($matchInfo['children']) && count($matchInfo['children']);
}

/**
 * 
 */
function template_preprocess_tourney_contestant(&$vars) {
  $plugin = &$vars['plugin'];
  
  $plugin->getPluginOptions();
  $options = $plugin->pluginOptions;
  
  $truncate = !empty($options['truncate']) ? $options['truncate'] : TOURNEY_TRUNCATE;
  
  // Let the plugin preprocess variables
  $plugin->preprocess('tourney-contestant', $vars);

  $vars['classes_array'][] = 'contestant-' . $vars['contestant']->slot;
  $vars['classes_array'][] = $vars['winner'] ? 'winner' : '';
  if ($vars['winner_set']) {
    $vars['classes_array'][] = $vars['winner'] ? 'winner' : 'loser';
  }
  
  if (property_exists($vars['contestant'], 'entity_name') && $vars['contestant']->entity_name) {
    $vars['name'] = $vars['contestant']->entity_name;
    $vars['classes_array'][] = 'eid-' . str_replace('.', '-', $vars['contestant']->eid);
  }
  else {
    $vars['name'] = 'Team ' . $vars['contestant']->slot;
  }
  
  if (strlen($vars['name']) > $truncate) {
    $vars['label'] = substr($vars['name'], 0, $truncate) . '...';
  }
  else {
    $vars['label'] = $vars['name'];
  }
  
  // Build the tourney container classes.
  $vars['classes'] = join(' ', $vars['classes_array']);
}