<?php

/**
 * @file
 * Forms to migrate books.
 */

/**
 * Form to define parameters for moving Books.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal from state
 *
 * @return array
 *   Drupal form
 */
function limerick_migrate_book_form($form, &$form_state) {
  $form = array();
  $form['content_type'] = array(
    '#title' => t('Text type'),
    '#type' => 'radios',
    '#options' => array(
      'book' => t('Book'),
      'newspaper' => t('Newspaper'),
    ),
    '#default_value' => 'book',
  );
  $form['book_dir'] = array(
    '#type' => 'textfield',
    '#title' => t("Directory"),
    '#description' => t('Full path to book or newspaper directory'),
  );
  $form['namespace'] = array(
    '#type' => 'textfield',
    '#title' => t("Namespace"),
    '#description' => t('Namespace for created objects'),
  );
  $form['collection'] = array(
    '#type' => 'textfield',
    '#title' => t("Collection"),
    '#description' => t('PID of collection to which object is being added'),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t("Ingest Content"),
  );
  return $form;
}

/**
 * Validation handler.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal form state
 */
function limerick_migrate_book_form_validate($form, &$form_state) {
  if (!is_dir($form_state['values']['book_dir'])) {
    // form_set_error('book_dir', t('Not a valid file path.'));
  }
  if (!islandora_object_load($form_state['values']['collection'])) {
    //form_set_error('collection', t('Collection does not exist.'));
  }
}

/**
 * Submit handler.
 *
 * @param array $form
 *   Drupal Form
 * @param array $form_state
 *   Drupal form state
 */
function limerick_migrate_book_form_submit(&$form, &$form_state) {
  module_load_include('inc', 'limerick', 'includes/utilities');
  $namespace = $form_state['values']['namespace'];
  $content_type = $namespace = $form_state['values']['content_type'];
  $collection_pid = $form_state['values']['collection'];
  $text_path = $form_state['values']['book_dir'];
  $path_parts = explode('/', $text_path);
  $title = array_pop($path_parts);
  $book_dir_path = implode('/', $path_parts);
  $object = limerick_construct_text_object($namespace, $title, $collection_pid, $content_type);
  $mods_path = "$book_dir_path/MODS/$title.xml";
  limerick_add_mods($object, $mods_path);
  $new_object = islandora_add_object($object);
  limerick_add_pages($new_object->id, $text_path, $content_type);
}

/**
 * Creates book object.
 *
 * @global type $user
 *
 * @param string $namespace
 *   Namepspoace for created object
 * @param string $title
 *   Book Title
 * @param string $parent_collection
 *   Collection pid for created book
 * @param string $content_type
 *   book or newspaper
 *
 * @return AbstractObject
 *   Book Object
 */
function limerick_construct_text_object($namespace, $title, $parent_collection, $content_type) {
  global $user;
  $mappings = array(
    'book' => 'islandora:bookCModel',
    'newspaper' => 'islandora:newspaperCModel',
  );
  $content_model = $mappings[$content_type];
  $fixed_title = str_replace('_', ' ', $title);
  $tuque = new IslandoraTuque();
  $object = $tuque->repository->constructObject($namespace);
  $object->relationships->add(FEDORA_RELS_EXT_URI, 'isMemberOf', $parent_collection);
  $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', $content_model);
  $object->label = $fixed_title;
  $object->owner = $user->name;
  return $object;
}

/**
 * Creates book pages and associates them with book object.
 *
 * @param string $text_object_pid
 *   PID of book
 * @param string $text_path
 *   Fole path to book directory
 */
function limerick_add_pages($text_object_pid, $text_path, $content_type) {
  $mappings = array(
    'book' => 'islandora:pageCModel',
    'newspaper' => 'islandora:newspaperIssueCModel',
  );
  $content_model = $mappings[$content_type];
  $dh = opendir($text_path);
  while ($filename = readdir($dh)) {
    $ext = pathinfo($filename, PATHINFO_EXTENSION);
    if (in_array($ext, array('tiff', 'tif'))) {
      $tiffs[] = "$text_path/$filename";
    }
  }
  sort($tiffs);
  $batch = array(
    'title' => t('Adding book pages'),
    'operations' => array(),
    'file' => drupal_get_path('module', 'limerick') . '/includes/limerick_book.inc',
    'progress_message' => t('@current of @total pages created.'),
  );
  $page_number = 1;
  foreach ($tiffs as $tiff) {
    $batch['operations'][] = array('limerick_create_page', array(
        'tiff' => $tiff,
        'page_number' => $page_number++,
        'text_object_pid' => $text_object_pid,
        'content_model' => $content_model,
      )
    );
  }
  $batch['operations'][] = array('limerick_update_book_thumbnail', array('book_pid' => $text_object_pid));
  batch_set($batch);
  batch_process();
}

/**
 * Creates individual page objects.
 *
 * @global type $user
 *
 * @param string $tiff
 *   File path to page image
 * @param string $page_number
 *   Page number to be assigned to page object
 * @param string $text_object_pid
 *   PID of book to which page is associated
 */
function limerick_create_page($tiff, $page_number, $text_object_pid, $content_model) {
  module_load_include('inc', 'islandora_ocr', 'includes/derivatives');
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  $namespace = substr($text_object_pid, 0, strpos($text_object_pid, ":"));
  global $user;
  $tuque = new IslandoraTuque();
  $repository = $tuque->repository;
  // Create object.
  $object = $repository->constructObject($namespace);
  $object->owner = $user->name;
  $object->label = pathinfo($tiff, PATHINFO_FILENAME);
  $ds_label = pathinfo($tiff, PATHINFO_BASENAME);
  islandora_paged_content_update_datastream($object, $tiff, 'OBJ', $ds_label, NULL, 'M', FALSE);
  $rels_ext = $object->relationships;
  $object->relationships->add(FEDORA_MODEL_URI, 'hasModel', $content_model);
  islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isPageOf', $text_object_pid);
  islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isSequenceNumber', $page_number, TRUE);
  islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isPageNumber', $page_number, TRUE);
  islandora_paged_content_set_relationship($rels_ext, ISLANDORA_RELS_EXT_URI, 'isSection', '1', TRUE);
  islandora_paged_content_set_relationship($rels_ext, FEDORA_RELS_EXT_URI, 'isMemberOf', $text_object_pid);
  islandora_paged_content_page_derive_image_datastreams($object);
  $ingested_object = islandora_add_object($object);
  $options = array(
    'preprocess' => FALSE,
  );
  islandora_ocr_derive_datastreams($ingested_object['OBJ'], $options);
}

/**
 * Updates thumbnail image for book object.
 *
 * @param string $text_object_pid
 *   Identifier for book object
 */
function limerick_update_book_thumbnail($text_object_pid) {
  module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
  $book_object = islandora_object_load($text_object_pid);
  islandora_paged_content_update_paged_content_thumbnail($book_object);
}
