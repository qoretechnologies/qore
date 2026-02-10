# -*- mode: qore; indent-tabs-mode: nil -*-
#! Qore WaveDataProvider module definition

/*  WaveDataProviderDefs.qc Copyright 2025 Qore Technologies, s.r.o.

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#! Contains all public definitions in the WaveDataProvider module
public namespace WaveDataProvider {

#! Wave app name constant
public const AppName = "Wave";

#! Default page size for pagination
public const DefaultPageSize = 50;

#! Customer fields fragment for GraphQL queries
public const CustomerFieldsFragment = "
    id
    name
    firstName
    lastName
    email
    phone
    fax
    mobile
    tollFree
    website
    internalNotes
    address {
        addressLine1
        addressLine2
        city
        province {
            code
            name
        }
        country {
            code
            name
        }
        postalCode
    }
    shippingDetails {
        name
        phone
        instructions
        address {
            addressLine1
            addressLine2
            city
            province {
                code
                name
            }
            country {
                code
                name
            }
            postalCode
        }
    }
    currency {
        code
    }
    createdAt
    modifiedAt
";

#! Invoice fields fragment for GraphQL queries
public const InvoiceFieldsFragment = "
    id
    createdAt
    modifiedAt
    pdfUrl
    viewUrl
    status
    title
    subhead
    invoiceNumber
    invoiceDate
    poNumber
    customer {
        id
        name
        email
    }
    currency {
        code
    }
    dueDate
    amountDue {
        value
        currency {
            code
        }
    }
    amountPaid {
        value
        currency {
            code
        }
    }
    taxTotal {
        value
        currency {
            code
        }
    }
    total {
        value
        currency {
            code
        }
    }
    exchangeRate
    footer
    memo
    disableCreditCardPayments
    disableBankPayments
    itemTitle
    unitTitle
    priceTitle
    amountTitle
    hideName
    hideDescription
    hideUnit
    hidePrice
    hideAmount
    lastSentAt
    lastSentVia
    lastViewedAt
    items {
        description
        quantity
        unitPrice
        subtotal {
            value
            currency {
                code
            }
        }
        total {
            value
            currency {
                code
            }
        }
        product {
            id
            name
        }
        account {
            id
            name
        }
        taxes {
            salesTax {
                id
                name
            }
            amount {
                value
            }
        }
    }
";

#! Product fields fragment for GraphQL queries
public const ProductFieldsFragment = "
    id
    name
    description
    unitPrice
    isSold
    isBought
    isArchived
    defaultSalesTaxes {
        id
        name
        rate
    }
    createdAt
    modifiedAt
";

#! Account fields fragment for GraphQL queries
public const AccountFieldsFragment = "
    id
    name
    description
    displayId
    type {
        name
        normalBalanceType
        value
    }
    subtype {
        name
        value
    }
    currency {
        code
    }
    isArchived
    sequence
";

#! Transaction fields fragment for GraphQL queries
public const TransactionFieldsFragment = "
    id
    date
    description
    postedOn
    createdAt
    modifiedAt
";

#! Business fields fragment for GraphQL queries
public const BusinessFieldsFragment = "
    id
    name
    type {
        name
        value
    }
    subtype {
        name
        value
    }
    currency {
        code
    }
    address {
        addressLine1
        city
        province {
            code
            name
        }
        country {
            code
            name
        }
        postalCode
    }
    phone
    createdAt
    modifiedAt
";

#! Sales tax fields fragment for GraphQL queries
public const SalesTaxFieldsFragment = "
    id
    name
    abbreviation
    description
    taxNumber
    rate
    isCompound
    isRecoverable
    isArchived
    createdAt
    modifiedAt
";

# ---- Queries ----

#! GraphQL query for listing user businesses
public const ListBusinessesQuery = "
query ListBusinesses {
    businesses {
        edges {
            node {" + BusinessFieldsFragment + "
            }
        }
    }
}
";

#! GraphQL query for listing customers with pagination
public const ListCustomersQuery = "
query ListCustomers($businessId: ID!, $page: Int, $pageSize: Int) {
    business(id: $businessId) {
        customers(page: $page, pageSize: $pageSize) {
            edges {
                node {" + CustomerFieldsFragment + "
                }
            }
            pageInfo {
                currentPage
                totalPages
                totalCount
            }
        }
    }
}
";

#! GraphQL query for getting a single customer by ID
public const GetCustomerQuery = "
query GetCustomer($businessId: ID!, $customerId: ID!) {
    business(id: $businessId) {
        customer(id: $customerId) {" + CustomerFieldsFragment + "
        }
    }
}
";

#! GraphQL query for listing invoices with pagination
public const ListInvoicesQuery = "
query ListInvoices($businessId: ID!, $page: Int, $pageSize: Int) {
    business(id: $businessId) {
        invoices(page: $page, pageSize: $pageSize) {
            edges {
                node {" + InvoiceFieldsFragment + "
                }
            }
            pageInfo {
                currentPage
                totalPages
                totalCount
            }
        }
    }
}
";

#! GraphQL query for listing products with pagination
public const ListProductsQuery = "
query ListProducts($businessId: ID!, $page: Int, $pageSize: Int) {
    business(id: $businessId) {
        products(page: $page, pageSize: $pageSize) {
            edges {
                node {" + ProductFieldsFragment + "
                }
            }
            pageInfo {
                currentPage
                totalPages
                totalCount
            }
        }
    }
}
";

#! GraphQL query for listing accounts with pagination
public const ListAccountsQuery = "
query ListAccounts($businessId: ID!, $page: Int, $pageSize: Int) {
    business(id: $businessId) {
        accounts(page: $page, pageSize: $pageSize) {
            edges {
                node {" + AccountFieldsFragment + "
                }
            }
            pageInfo {
                currentPage
                totalPages
                totalCount
            }
        }
    }
}
";

#! GraphQL query for listing sales taxes
public const ListSalesTaxesQuery = "
query ListSalesTaxes($businessId: ID!) {
    business(id: $businessId) {
        salesTaxes {
            edges {
                node {" + SalesTaxFieldsFragment + "
                }
            }
        }
    }
}
";

#! GraphQL query for getting a single invoice by ID
public const GetInvoiceQuery = "
query GetInvoice($businessId: ID!, $invoiceId: ID!) {
    business(id: $businessId) {
        invoice(id: $invoiceId) {" + InvoiceFieldsFragment + "
        }
    }
}
";

#! GraphQL query for getting a single product by ID
public const GetProductQuery = "
query GetProduct($businessId: ID!, $productId: ID!) {
    business(id: $businessId) {
        product(id: $productId) {" + ProductFieldsFragment + "
        }
    }
}
";

#! GraphQL query for the authenticated user
public const UserQuery = "
query User {
    user {
        id
        firstName
        lastName
        defaultEmail
    }
}
";

# ---- Mutations ----

#! GraphQL mutation for creating a customer
public const CustomerCreateMutation = "
mutation CustomerCreate($input: CustomerCreateInput!) {
    customerCreate(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
        customer {" + CustomerFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for updating a customer
public const CustomerPatchMutation = "
mutation CustomerPatch($input: CustomerPatchInput!) {
    customerPatch(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
        customer {" + CustomerFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for deleting a customer
public const CustomerDeleteMutation = "
mutation CustomerDelete($input: CustomerDeleteInput!) {
    customerDelete(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
    }
}
";

#! GraphQL mutation for creating a product
public const ProductCreateMutation = "
mutation ProductCreate($input: ProductCreateInput!) {
    productCreate(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
        product {" + ProductFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for updating a product
public const ProductPatchMutation = "
mutation ProductPatch($input: ProductPatchInput!) {
    productPatch(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
        product {" + ProductFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for creating a sales tax
public const SalesTaxCreateMutation = "
mutation SalesTaxCreate($input: SalesTaxCreateInput!) {
    salesTaxCreate(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
        salesTax {" + SalesTaxFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for creating an account
public const AccountCreateMutation = "
mutation AccountCreate($input: AccountCreateInput!) {
    accountCreate(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
        account {" + AccountFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for creating a money transaction
public const MoneyTransactionCreateMutation = "
mutation MoneyTransactionCreate($input: MoneyTransactionCreateInput!) {
    moneyTransactionCreate(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
        transaction {" + TransactionFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for creating an invoice
public const InvoiceCreateMutation = "
mutation InvoiceCreate($input: InvoiceCreateInput!) {
    invoiceCreate(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
        invoice {" + InvoiceFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for approving an invoice
public const InvoiceApproveMutation = "
mutation InvoiceApprove($input: InvoiceApproveInput!) {
    invoiceApprove(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
        invoice {" + InvoiceFieldsFragment + "
        }
    }
}
";

#! GraphQL mutation for sending an invoice
public const InvoiceSendMutation = "
mutation InvoiceSend($input: InvoiceSendInput!) {
    invoiceSend(input: $input) {
        didSucceed
        inputErrors {
            code
            message
            path
        }
        invoice {" + InvoiceFieldsFragment + "
        }
    }
}
";

#! Wave logo in SVG format (loaded from file)
public const WaveLogo = File::readTextFile(get_script_dir() + "/wave-logo.svg");

} # namespace WaveDataProvider
