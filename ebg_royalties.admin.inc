<?php

/**
 * @File
 * Administration pages for ebg royalties module.
 */

define('EBG_ROYALTIES_PAYMENT_DATE_FORMAT', 'm/d/Y H:i:s');

function ebg_royalties_dashboard() {
  // Setup table headers.
  $header = array(
    t('Author'),
    t('Royalites'),
    t('Payment Type'),
    t('Actions'),
  );

  // Get Authors
  $query = 'SELECT DISTINCT(ur.uid)
    FROM {users_roles} AS ur
    WHERE ur.rid IN (:rids)';
  $result = db_query($query, array(':rids' => array(4)));
  $uids = $result->fetchCol();
  $users = user_load_multiple($uids);

  // Setup table rows
  $rows = array();
  foreach ($users as $uid => $user) {
    $royalties = _ebg_royalties_by_user($user->uid);
    $net_royalties = isset($royalties['net_royalties']['amount_formatted']) ? $royalties['net_royalties']['amount_formatted'] : '$0.00';
    $actions = l(t('Pay out'), 'admin/config/ebg/royalties/payments',
      array('query' => array(
        'uid' => $user->uid,
        'payment_amount' => substr($net_royalties, 1, strlen($net_royalties)),
      )
    ));

    $rows[] = array(
      l(check_plain($user->name), 'user/' . $user->uid),
      check_plain($net_royalties),
      check_plain(''),
      $actions,
    );
  }

  // Sort rows by royalties owed
  Usort($rows, function ($a, $b)  {
    $a_amount = (float) substr($a[1], 1, strlen($a[1]));
    $b_amount = (float) substr($b[1], 1, strlen($b[1]));

    if ($a_amount === $b_amount) {
      return 0;
    }

    return ($a_amount > $b_amount) ? -1 : 1;
  });

  return theme('table', array('header' => $header, 'rows' => $rows, 'empty' => t('No users.')));
}

function ebg_royalties_payments($id = NULL) {
  // Setup table headers.
  $header = array(
    t('User'),
    t('Amount'),
    t('Date'),
    t('Notes'),
    t('Actions'),
  );

  // Get messages
  $query = db_select('ebg_payments', 'rp')->extend('PagerDefault');
  $query->fields('rp')->orderBy('rp.pid', 'DESC')->limit(50);
  $result = $query->execute();

  // Setup table rows
  $rows = array();
  while ($payment = $result->fetchObject()) {
    $user = user_load($payment->uid);

    $actions = l(t('Edit'), 'admin/config/ebg/royalties/payments/' . $payment->pid . '/edit');
    $actions .= ' | '  . l(t('Delete'), 'admin/config/ebg/royalties/payments/' . $payment->pid . '/delete');
    $actions .= '<br>' . t('Created: @date', array('@date' => date(EBG_ROYALTIES_PAYMENT_DATE_FORMAT, $payment->created)));
    $actions .= '<br>' . t('Updated: @date', array('@date' => date(EBG_ROYALTIES_PAYMENT_DATE_FORMAT, $payment->updated)));

    $rows[] = array(
      check_plain($user->name),
      check_plain($payment->amount),
      check_plain(date(EBG_ROYALTIES_PAYMENT_DATE_FORMAT, $payment->date)),
      check_plain($payment->notes),
      $actions,
    );
  }

  // Fully rendered table of messages.
  $current_messages = theme('table', array('header' => $header, 'rows' => $rows, 'empty' => t('No messages.'))).theme('pager');

  // Fully rendered message edd/edit form.
  $payment_form = drupal_get_form('ebg_royalties_payment_form', $id);

  return render($payment_form) . '<h2>Royalty Payments</h2>' . $current_messages;
}

/**
 * Form for adding/editing a message.
 */
function ebg_royalties_payment_form($form, &$form_state, $id = NULL) {
  if (isset($id)) {
    $payment = _ebg_royalties_payment_get($id);

    $form['id'] = array(
      '#type' => 'hidden',
      '#value' => $id,
    );
  }
  else {
    $payment = new stdClass();
    $payment->uid = isset($_GET['uid']) ? check_plain($_GET['uid']) : NULL;
    $payment->amount = isset($_GET['payment_amount']) ? check_plain($_GET['payment_amount']) : NULL;
  }

  $form['title'] = array(
    '#markup' => isset($id) ? t('Edit payment') : t('Add payment'),
    '#prefix' => '<h2>',
    '#suffix' => '</h2>'
    );

  $form['user'] = array(
    '#type' => 'textfield',
    '#title' => t('User ID'),
    '#default_value' => isset($payment->uid) ? $payment->uid : '',
    '#required' => TRUE,
    '#description' => t('The authors user id the payment is made to.'),
  );

  $form['amount'] = array(
    '#type' => 'textfield',
    '#title' => t('Payment amount'),
    '#default_value' => isset($payment->amount) ? $payment->amount : '',
    '#required' => TRUE,
    '#description' => t('The amount paid to author. Use negative numbers to indicate a credit.'),
  );

  $form['date'] = array(
    '#type' => 'textfield',
    '#title' => t('Payment date'),
    '#default_value' => isset($payment->date) ? date(EBG_ROYALTIES_PAYMENT_DATE_FORMAT, $payment->date) : date(EBG_ROYALTIES_PAYMENT_DATE_FORMAT, time()),
    '#required' => TRUE,
    '#description' => t('When the payment was made. Format of @format.', array('@format' => 'MM/DD/YYYY HH:MM:SS')),
  );

  $form['notes'] = array(
    '#type' => 'textarea',
    '#title' => t('Notes'),
    '#default_value' => isset($payment->notes) ? $payment->notes : '',
    '#required' => TRUE,
    '#description' => t('Any administrative notes about the payment.'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}

/**
 * Adds or saves a message.
 * @see ebg_royalties_payment_form()
 */
function ebg_royalties_payment_form_submit($form, &$form_state) {
  $data = array(
    'notes' => $form_state['values']['notes'],
    'date' => strtotime($form_state['values']['date']),
    'uid' => $form_state['values']['user'],
    'amount' => $form_state['values']['amount'],
    'updated' => time(),
  );

  if (isset($form_state['values']['id'])) {
    // Update message.
    $data['pid'] = $form_state['values']['id'];
    drupal_write_record('ebg_payments', $data, 'pid');
    $form_state['redirect'] = 'admin/config/ebg/royalties/payments';

    drupal_set_message('Payment updated successfully.');
  }
  else {
    // New message.
    $data['created'] = $data['updated'];
    drupal_write_record('ebg_payments', $data);

    drupal_set_message('Payment saved successfully.');
  }
}

/**
 * Form for deleting a message.
 */
function ebg_royalties_payment_delete_form($form, &$form_state, $id) {
  $form['id'] = array(
    '#type' => 'hidden',
    '#value' => $id
  );

  $payment = _ebg_royalties_payment_get($id);
  $user = user_load($payment->uid);

  $form['msg'] = array(
    '#markup' => t('Payment to @user on @date for @amount.', array(
      '@user' => $user->name,
      '@date' => date(EBG_ROYALTIES_PAYMENT_DATE_FORMAT, $payment->date),
      '@amount' => $payment->amount,
    )),
    '#prefix' => '<p>',
    '#suffix' => '</p>',
  );

  return confirm_form($form, t('Are you sure you want to delete this payment?'), 'admin/config/ebg/royalties/payments');
}

/**
 * Deletes a message.
 * @see ebg_royalties_payment_delete_form()
 */
function ebg_royalties_payment_delete_form_submit($form, &$form_state) {
  db_query('delete from {ebg_payments} where pid = :pid', array(':pid' => $form_state['values']['id']));
  $form_state['redirect'] = 'admin/config/ebg/royalties/payments';

  drupal_set_message('Payment deleted successfully.');
}
