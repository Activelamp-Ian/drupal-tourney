<?php

/**
 * @file
 * Tournament class.
 */

class TourneyTournamentEntity extends TourneyEntity {

  public  $matchIds = NULL;
  public  $gameIds = NULL;
  public  $roundMatchIds = NULL;
  public  $uri;
  public  $data;
  public  $contestants = NULL;
  private $tournamentStarted  = NULL;
  private $tournamentFinished = NULL;
  private $tournamentWinner = NULL;
  private $tournamentMatches = NULL;
  public  $status;
  public  $path = array();

  public $tourneyFormatPlugin = NULL;

  public function __construct($values = array(), $entityType = NULL) {
    parent::__construct($values, $entityType);
    $this->type = "tournament";
    // Instantiate the plugin, but only if our entity has been created
    // via entity_load(). If our entity has been constructed by another
    // means, like entity_create(), required properties will not yet be
    // available.
    // @todo : Lazy load this?
    if (!empty($this->players)) {
      $tourneyFormatPlugin = $this->format;
      if (!class_exists($tourneyFormatPlugin)) {
        throw new Exception(t('Tourney format plugin does not exist'));
      }
      $this->tourneyFormatPlugin = new $tourneyFormatPlugin($this->players, $this);

      // Add a url to the object
      $this->uri = $this->getUri();
      
      $this->name_original = $this->name;
    }
  }

  /**
   * Static function that returns an array of match ids for that passed in
   * tournament id.
   *
   * @param $tournament_id
   *   The tournament entity id to find matches on.
   * @return array
   *   An array of match entity ids.
   */
  public static function fetchMatchIds($tournament_id) {
    $query = tourney_relation_query('tourney_tournament', $tournament_id);
    $query->entityCondition('bundle', 'has_match');
    $results = $query->execute();

    $match_ids = array();
    foreach ($results as $relation) {
      $r = relation_load($relation->rid);
      $match_ids[] = $r->endpoints[LANGUAGE_NONE][1]['entity_id'];
    }

    return $match_ids;
  }

  /**
   * Report all tournaments.
   */
  public static function fetchTournaments() {
    return entity_load('tourney_tournament', FALSE, array(), FALSE);
  }

  /**
   * Constructs a form options list containing all tournament names.
   *
   * @return array
   *   Associative array keyd by tournament identifier and valued with
   *   the tournament label on success. An empty array on failure or non
   *   found.
   */
  public static function formOptionsTournaments() {
    $options = array();
    $tournaments = TourneyTournamentEntity::fetchTournaments();
    foreach ($tournaments as $tournament) {
      $options[$tournament->id] = $tournament->label;
    }
    return $options;
  }

  /**
   * Static function that returns an array of game ids for that passed in
   * tournament id.
   *
   * @param $tournament_id
   *   The tournament entity id to find matches on.
   * @return array
   *   An array of match entity ids.
   */
  public static function fetchGameIds($tournament_id) {
    $match_ids = TourneyTournamentEntity::fetchMatchIds($tournament_id);

    $game_ids = array();
    foreach ($match_ids as $mid) {
      $game_ids = TourneyMatchEntity::fetchGameIds($mid);
    }

    return $game_ids;
  }

  /**
   * Lazy load all the match ids for this tournament. Once the matches have
   * been loaded for the tournament, it will return the 'cached' copy.
   */
  public function getMatchIds() {
    if (empty($this->matchIds)) {
      $this->matchIds = TourneyTournamentEntity::fetchMatchIds($this->id);
    }
    return $this->matchIds;
  }

  /**
   * Retrieve a match by machine name.
   *
   * @param $mid
   *   The match number to get
   * @param $reset
   *   Reset the static entity cache.
   * @return
   *   A fully loaded match object.
   */
  public function getMatchById($mid, $reset = FALSE) {
    return tourney_entity_load_by_name('tourney_match', $this->name . '_match-' . $mid, $reset);
  }

  public function getHeadToHeadMatchIds($contestant1, $contestant2) {
    if ($contestant1->entity_id > $contestant2->entity_id) {
      $temp = $contestant1;
      $contestant1 = $contestant2;
      $contestant2 = $temp;
    }

    $cache_id = 'getHeadToHeadMatchIds:' . $this->id . ':' . $contestant1->eid . ':' . $contestant2->eid;
    if (!$cache = cache_get($cache_id, 'cache_tourney')) {
      $match_ids = $this->getMatchIds();

      $query = tourney_relation_query($contestant1->entity_type, $contestant1->entity_id);
      $query->fieldCondition('endpoints', 'entity_type', 'tourney_match', '=');
      $query->fieldCondition('endpoints', 'entity_id', $match_ids, 'IN');

      $relation_ids = array_keys($query->execute());
      if (empty($relation_ids)) return;
      $relations = entity_load('relation', $relation_ids);
      $match_ids = array();
      foreach ($relations as $relation) {
        $match_ids[] = $relation->endpoints[LANGUAGE_NONE][0]['entity_id'];
      }
      $query = tourney_relation_query($contestant2->entity_type, $contestant2->entity_id);
      $query->fieldCondition('endpoints', 'entity_type', 'tourney_match', '=');
      $query->fieldCondition('endpoints', 'entity_id', $match_ids, 'IN');

      $relation_ids = array_keys($query->execute());
      $relations = entity_load('relation', $relation_ids);

      $match_ids = array();
      foreach ($relations as $relation) {
        $match_ids[] = $relation->endpoints[LANGUAGE_NONE][0]['entity_id'];
      }
      cache_set($cache_id, $match_ids, 'cache_tourney');
    }
    else {
      $match_ids = $cache->data;
    }

    return $match_ids;
  }
  
  /**
   * Creates multi-dimensional array of rounds with corresponding matches in each round.
   * 
   * @return
   *   Returns an array of rounds with match mids of each round as the value.
   */
  public function getRoundMatchIds() {
    if (is_null($this->roundMatchIds)) {
      $plugin = $this->tourneyFormatPlugin;
      $plugin->build();
      foreach($plugin->data['matches'] as $mid => $match_info) {
        $this->roundMatchIds[$match_info['round']][] = $mid;
      }
    }
    return $this->roundMatchIds;
  }

  /**
   * Retrieve a match by machine name.
   *
   * @param $mid
   *   The match number to get
   * @return
   *   The URI to the match as a string.
   */
  public function getMatchURI($mid) {
    $entity = $this->getMatchById($mid);

    return is_object($entity) ? $entity->uri : NULL;
  }

  /**
   * Lazy load all the game ids for this tournament. Once the games have been
   * loaded for the tournament, it will return the 'cached' copy.
   */
  public function getGameIds() {
    if (empty($this->gameIds)) {
      $this->gameIds = TourneyMatchEntity::fetchGameIds($this->getMatchIds());
    }
    return $this->gameIds;
  }

  /**
   * Report current winner rankings for this tournament.
   *
   * @return array
   *   associative array.
   *
   * @see TourneyMatch::fetchRanks()
   */
  public function fetchRanks() {
    $ranks = array();
    $ranks['game_wins'] = array();
    $ranks['match_wins'] = array();
    $ranks['match_loss'] = array();
    $ranks['contestants'] = array();

    // Load all the matches in this tournament.
    $matches = tourney_match_load_multiple($this->getMatchIds());

    if (!empty($matches)) {
      foreach ($matches as $match) {
        // Compile array of game wins keyed by [match_id][contestant].
        $ranks['game_wins'][$match->id] = $match->fetchRanks();
        // Compile array of all contestants in this tournament.
        $ranks['contestants'] = array_merge($ranks['contestants'], $ranks['game_wins'][$match->id]);
      }
      $ranks['contestants'] = array_keys($ranks['contestants']);
    }

    if (!empty($matches)) {
      foreach ($ranks['contestants'] as $contestant) {
        // Initialize all array keys to 0. Avoiding php notices further down.
        $ranks['match_wins'][$contestant] = 0;
        $ranks['match_loss'][$contestant] = 0;
      }
      $ranks['match_wins']['NA'] = 0;
      $ranks['match_loss']['NA'] = 0;
      foreach ($matches as $match) {
        // Sum total number of wins and losses keyed by contestant.
        $keys = array_keys($ranks['game_wins'][$match->id]);
        $keys = (empty($keys)) ? array('NA', 'NA') : $keys;
        $ranks['match_wins'][$keys[0]] += 1;
        $ranks['match_loss'][$keys[1]] += 1;
      }
      // Sort the arrays by descending values.
      arsort($ranks['match_wins']);
      arsort($ranks['match_loss']);
    }

    return $ranks;
  }

  /**
   * Stub code for detemining if a tournament has started
   */
  public function isStarted() {
    // @todo: do something
  }

  /**
   * Report if a tournament is finished.
   *
   * @param TourneyTournamentEntity $tournament
   *
   * @return bool $finished
   *   Will report TRUE if the tournament is finished, FALSE if not.
   */
  function isFinished() {
    $matches = tourney_match_load_multiple($this->getMatchIds());
    if (!empty($matches)) {
      foreach ($matches as $match) {
        // Delegate the checking to the match to see if each match is finished
        if ($match->status == 1 && !$match->isFinished()) {
          return FALSE;
        }
      }
      return TRUE;
    }
    throw new Exception(t('There are no matches for this tournament'));
  }

  /**
   * Set the winner property to the winner of this match.
   */
  public function determineWinner() {
    if ($this->isFinished()) {
      // With roundrobin, the player with the most wins, wins.
      if ($this->format == 'RoundRobinController') {
        $standings = $this->getStandings();
        // todo : remove quick hack, implement custom uasort callback.
        foreach ($standings as $key => $standing) {
          $winners[$key] = $standing['wins'];
        }
        arsort($winners);

        $keys = array_keys($winners);
        $this->winner = $keys[0];
      }
      else {
        // Otherwise, whoever won the last match wins.
        if ($last_match = $this->lastMatch()) {
          $this->winner = $last_match->winner;
        }
      }
      
      // Save
      $this->save();
    }
    else {
      // In the case that not all matches are finished, reset the winner.
      if ($this->winner) {
        $this->winner = NULL;
        $this->save();
      }
    }

    return $this;
  }
  
  /**
   * Get the last published match in the tournament.
   * 
   * @return $entity
   *   Returns the full loaded last match of the tournament.
   */
  public function lastMatch() {
    $matches = array_reverse($this->tourneyFormatPlugin->data['matches']);
    foreach ($matches as $match) {
      $entity = $this->getMatchById($match['id'], TRUE);
      if ($entity->status) {
        return $entity;
      }
    }
    return NULL;
  }

  /**
   * Delete a tournament.
   */
  public function delete() {
    // Find all the relationships to matches for this tournament.
    $mquery = tourney_relation_query('tourney_tournament', $this->id)->entityCondition('bundle', 'has_match');
    $matches = $mquery->execute();

    // Build an array of stuff to delete.
    foreach (array_keys($matches) as $rid) {
      // Get all the match relations and matches and delete them.
      $relation = relation_load($rid);
      relation_delete($rid);
      $match = tourney_match_load($relation->endpoints['und'][1]['entity_id']);
      if (is_object($match)) {
        $match->delete();
      }
      else {
        drupal_set_message(t('Could not delete match entity %id. Does not exist', array(
         '%id' => $relation->endpoints['und'][1]['entity_id'],
        )), 'warning');
      }
    }
    
    // Remove any ranks from the tourney_rank table.
    db_delete('tourney_rank')
      ->condition('tournament', $this->id)
      ->execute();

    parent::delete();
  }

  /**
  * Save the entity
  */
  public function save() {
    // Store the is_new property in local variable. The property is no longer
    // there after the entity is initially saved.
    $new = property_exists($this, 'is_new') ? $this->is_new : FALSE;
    parent::save();

    // Clear any necessary caches
    cache_clear_all('getContestants:' . $this->eid, 'cache_tourney');

    // Create the matches and first game if new tournament.
    if ($new) {
      $controller = TourneyController::loadPlugin($this);
      $controller->build();
      
      // saveMatches isn't part of the plugin interface. We need to make sure
      // sure the method exists before calling.
      if (method_exists($controller, 'saveMatches')) {
        $controller->saveMatches();
      }

      // Update the tournament with the number of matches created
      $this->matches_created = count($this->getMatchIds());
      drupal_write_record('tourney_tournament', $this, 'id');
    }
    else {
      if ($this->name_original !== $this->name) {
        $mquery = tourney_relation_query('tourney_tournament', $this->id)->entityCondition('bundle', 'has_match');
        $matches = $mquery->execute();
        foreach (array_keys($matches) as $rid) {
          $relation = relation_load($rid);
          $match = tourney_match_load($relation->endpoints['und'][1]['entity_id']);
          if (!$match) continue;
          $match->name = $this->name . substr($match->name, strpos($match->name, '_'));
          $match->save();
        }
      }
    }
  }

  /**
   * Gets a list of participating contestants in seed order from the first round.
   *
   * @return array $contestants
   *   List of contestants, all entities from the first round of a tournament.
   */
  public function getContestants() {
    if (property_exists($this, 'is_new') && $this->is_new) return array();
    
    $cid = __FUNCTION__ . ':' . $this->eid;
    if (!$cache = cache_get($cid, 'cache_tourney')) {
      $contestant_eids = array_keys($this->get('seed_positions'));
      $this->contestants = entity_load_by_eid($contestant_eids);
    }
    else {
      $this->contestants = $cache->data;
    }
    cache_set($cid, $this->contestants, 'cache_tourney');
    return $this->contestants;
  }
  
  /**
   * Traverse all matches and get all matches a contestant is on.
   * 
   * This method finds all contestants by loading each match. The results are cached.
   */
  public function getContestantMatches($contestant_eid) {
    $cid = __FUNCTION__ . ':' . $this->id . ':' . $contestant_eid;
    if (!$cache = cache_get($cid, 'cache_tourney')) {
      $match_ids = $this->getMatchIds();
      $matches = array();
      foreach ($match_ids as $match_id) {
        $match = tourney_match_load($match_id);
        $contestants = $match->getMatchContestants();
        if (array_key_exists($contestant_eid, $contestants)) {
          $matches[$match_id] = $match_id;
        }
      }
      if (!empty($matches)) {
        cache_set($cid, $matches, 'cache_tourney');
      }
    }
    else {
      $matches = $cache->data;
    }
    return $matches;
  }
  
  /**
   * Checks to see if a contestant is still active in any match in tournament.
   * 
   * Look at every match the contestant passed in is on. If the contestant is
   * on any match that does NOT have a winner, then the contestant is still
   * active on the tournament.
   * 
   * @param $contestant_eid
   *   The entity and id of the contestant to lookup.
   * @return bool
   *   Returns True of the contestant is still active.
   */
  public function isContestantActive($contestant_eid) {
    $matches = $this->getContestantMatches($contestant_eid);
    
    foreach($matches as $match_id) {
      $match = tourney_match_load($match_id);
      if (empty($match->winner)) {
        return true;
      }
    }
    return false;
  }
  
  /**
   * Pull all matches in and returns a list of all the tallies of wins/losses
   *
   * @return array $standings
   *   Keyed array with eid as the key and array of data as the value
   *   $standings['eid'] = array(
   *     'wins'   => 0,
   *     'losses' => 0,
   *     'total'  => 0,
   *     'first'  => 0, // First is used for the rare case of ties, just to see who won first
   *   )
   */
  public function getStandings() {
    $matches = $this->getMatchIds();
    $standings = array();
    $i = 0;
    foreach ($matches as $match) {
      $match = entity_load_single('tourney_match', $match);

      foreach ($match->getContestantIds() as $var => $contestant) {
        // If the standings array doesn't have this contestant yet, add it
        if (!array_key_exists($eid = $contestant['eid'], $standings)) {
          $standings[$eid] = array('wins' => 0, 'losses' => 0, 'total' => 0, 'first' => -1);
        }

        // If a match hasn't been won yet, don't count its games
        if (!$match->getWinner()) {
          continue;
        }

        // Tally either as a winner or a loser, then up the total matches played
        if ($match->getWinner() == $eid) {
          $standings[$eid]['wins']++;
        }
        else {
          $standings[$eid]['losses']++;
        }
        $standings[$eid]['total']++;
        // First is used for the rare case of ties, just to see who won first
        if ($standings[$eid]['first'] == -1) {
          $standings[$eid]['first'] = $i++;
        }
      }
    }
    return $standings;
  }

  /**
   * Renders the bracket of the tournament.
   * 
   * If the rendering is not dependent on the drupal entity system pass the
   * actual rendering logic to the plugin, otherwise specify the style in the
   * switch statement before default is reached, and return your own output.
   * 
   * @param $style
   *   The style to render the tournament. Possible values are: tree or list.
   *   Defaults to tree.
   * 
   * @see tourney_tourney_tournament_view().
   */
  public function render($style) {
    switch($style) {
      // Specify the render output that is dependent on entities here.
      case 'list' : return views_embed_view('tourney_matches_list');
      // Pass everything else to the plugin to render.
      default : return $this->tourneyFormatPlugin->render($style);
    }
  }

  /**
   * Function to determine if a match can be edited
   *
   * @param $mid
   *   The match machine name from the plugin
   * @return
   *   Bitwise value with contestant slots turned on (slot 1 in right most position)
   */
  public function canSetContestants($mid) {
    $return = 0;
    $match = $this->getMatchById($mid);

    // If no automation is turned on, we should be able to edit both contestants.
    if ($this->get('no_automation', FALSE)) {
      return 0x3;
    }
    
    if ((user_access('administer tourney') == FALSE && user_access("edit tourney") == FALSE)
      || count($match->getGameIds()) > 1) {
      return FALSE;
    }
    $match->getMatchInfo();
    $round = $match->matchInfo['round'];
    
    if (!array_key_exists('canSetContestants', $match->matchInfo)) {
      // Return a bitwise value with contestant slots turned on (slot 1 in right most position)
      if ($round == 1 && $match->matchInfo['bracket'] === 'main') {
        // Both slots can be set
        $match->matchInfo['canSetContestants'] = 0x3;
      }
      elseif ($match->matchInfo && array_key_exists('seeds', $match->matchInfo)) {
        foreach (array(1,2) as $slot) {
          if (array_key_exists($slot, $match->matchInfo['seeds'])) {
            $match->matchInfo['canSetContestants'] += $slot;
          }
        }
      }
    }

    return $match->matchInfo['canSetContestants'];
  }

  /**
   * Rules called function, usually, to run win code on a match. Calls the plugin
   * specific callback, if there, then checks internally if the tournament's been
   * given a winner
   *
   * @param $match
   *   match object to run win code on
   */
  public function matchIsWon($match) {
    $old_winner = $match->winner;
    $plugin = $this->tourneyFormatPlugin;
    $info = $match->matchInfo;
    // $match->cleanGames();

    module_load_include('inc', 'tourney', 'includes/tourney.rank');
    if (!$this->get('no_automation', FALSE) && $this->format != 'RoundRobinController') {
      if (!$match->isFinished()) {
        return;
      }
      // clear winner and loser so we can get a fresh winner, just in case
      $match->clearWinner();

      if ($old_winner && $old_winner !== $match->getWinner()) {
        tourney_rank_win($this, $match->getLoserEntity(), TRUE);
        tourney_rank_lose($this, $match->getWinnerEntity(), TRUE);
      }

      $winner = $match->getWinnerEntity();
      $loser = $match->getLoserEntity();
      // this code here lets the champion round rule fire before setting winners
      // on the tiebreaker, in case the tiebreaker in unnecessary
      $match->save();

      try {
        $winnerMatch = $match->nextMatch('winner');
        if ($winnerMatch && $winnerMatch->status == 1) {
          $slot = $match->matchInfo['nextMatch']['winner']['slot'];
          $winnerMatch->queueAddContestant($winner, $slot)->save();
        }
        $loserMatch = $match->nextMatch('loser');
        if ($loserMatch && $loserMatch->status == 1) {
          $slot = $match->matchInfo['nextMatch']['loser']['slot'];
          $loserMatch->queueAddContestant($loser, $slot)->save();
        }
      } catch (TourneyMatchException $e) {
        // Do nothing, just let the exception write to watchdog.
      }
    }
    else {
      $match->determineWinner();
      $match->save();
    }
    if (!$old_winner || ($old_winner && $old_winner !== $match->getWinner())) {
      tourney_rank_win($this, $match->getWinnerEntity());
      tourney_rank_lose($this, $match->getLoserEntity());
    }
    
    
    $this->determineWinner();
  }

  /**
   * Rules Action Callback: Recursive function that sets players in RoundRobin.
   * 
   * Player positions in roundrobin tournaments are based on the position they
   * enter the tournament, not on whether or not a game is won or lost. Set
   * players in all matches once the first round has been setup.
   *
   * @param $mid
   *   The match id from the plugin.
   * @param $slot
   *   The slot number to process. Defaults to processing both slots.
   */
  public function populateRoundRobinMatches($mid, $slot = '') {
    $match = $this->getMatchById($mid);
    $match->getMatchInfo();
    if (array_key_exists('nextMatch', $match->matchInfo)) {
      foreach ($match->getMatchContestants() as $contestant) {
        // If a slot was specified, ensure only that slot is processed.
        if ($slot && $contestant->slot != $slot) continue;

        // It's possible for a manual upload schema to specify a next match
        // for the first or second contestant but not the other.
        if (!isset($match->matchInfo['nextMatch'][$contestant->slot]['id']))
          continue;

        $next = $match->matchInfo['nextMatch'][$contestant->slot]['id'];
        $next_slot = $match->matchInfo['nextMatch'][$contestant->slot]['slot'];

        if ($next === NULL) continue;

        $next_match = $this->getMatchById($next);
        try {
          $next_match->queueAddContestant($contestant, $next_slot)->save();
        } catch (TourneyMatchException $e) {
          // Do nothing, just let the exception write to watchdog.
        }

        // Recurse
        $this->populateRoundRobinMatches($next, $next_slot);
      }
    }
  }

  /**
   * Get settings from tourney settings.
   *
   * @param $name
   *   The name of the setting to retrieve, or pass in **ALL** to retrieve all
   *   settings for the current tournament.
   * @param $default
   *   If the setting does not exists, the default value to return.
   */
  public function get($name, $default = NULL) {
    global $_tourney_config;
    if (!isset($_tourney_config)) {
      tourney_initialize_configuration();
    }

    if (!property_exists($this, 'entity_id')) {
      if ($name == '**ALL**') {
        return isset($this->config) ? $this->config : array();
      }
      return isset($this->config[$name]) ? $this->config[$name] : (isset($this->id) && isset($_tourney_config[$this->id][$name]) ? $_tourney_config[$this->id][$name] : $default);
    }

    if ($name == '**ALL**') {
      return !empty($this->id) && !empty($_tourney_config[$this->id])
        ? $_tourney_config[$this->id] : array();
    }
    return !empty($this->id) && !empty($_tourney_config[$this->id][$name])
      ? $_tourney_config[$this->id][$name] : $default;
  }

  /**
   * Set a setting in tourney configuration table
   *
   * @param $name
   *   The name of the setting to store.
   * @param $value
   *   The value to store for the setting.
   */
  public function set($name, $value) {
    global $_tourney_config;
    if (!isset($_tourney_config)) tourney_initialize_configuration();

    db_merge('tourney')
      ->key(array(
        'id'   => $this->id,
        'type' => $name,
      ))
      ->fields(array(
        'value' => serialize($value)
      ))
      ->execute();

    cache_clear_all('configuration', 'cache_tourney');

    $_tourney_config[$this->id][$name] = $value;
    $this->config[$name] = $value;
  }
}