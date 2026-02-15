# -*- mode: qore; indent-tabs-mode: nil -*-
#! @file AppGroup.qc Defines the AppGroup enum for categorizing data provider applications

/*  AppGroup.qc Copyright 2019 - 2026 Qore Technologies, s.r.o.

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

#! Main DataProvider namespace
public namespace DataProvider {
    #! Test constant to verify file is being loaded
    public const AppGroupTestConst = "test_value";

    #! Data Provider Application Group enum
    /** Provides type-safe application group names for categorizing data provider applications.

        Access as @code DataProvider::AppGroup::Messaging @endcode from modules that require DataProvider.
    */
    public enum AppGroup : string {
        #! Accounting & ERP systems
        AccountingErp = "Accounting & ERP",
        #! AI & Language Models
        AiLlm = "AI & Language Models",
        #! Analytics & Reporting tools
        Analytics = "Analytics & Reporting",
        #! API & Integration tools
        ApiIntegration = "API & Integration",
        #! Cloud Storage & File Management
        CloudStorage = "Cloud Storage & File Management",
        #! CRM & Sales Management
        CrmSales = "CRM & Sales Management",
        #! Customer Support & Helpdesk
        CustomerSupport = "Customer Support & Helpdesk",
        #! Databases & Backend Services
        Databases = "Databases & Backend Services",
        #! Data Transformation utilities
        DataTransformation = "Data Transformation",
        #! Design & Creative Tools
        DesignCreative = "Design & Creative Tools",
        #! DevOps & Cloud Infrastructure
        DevOps = "DevOps & Cloud Infrastructure",
        #! Document Signing & Contracts
        DocumentSigning = "Document Signing & Contracts",
        #! Documents & Documentation
        Documents = "Documents & Documentation",
        #! E-commerce Platforms
        Ecommerce = "E-commerce Platforms",
        #! Email & Email Marketing
        Email = "Email & Email Marketing",
        #! File System & Local Storage
        FileSystem = "File System & Local Storage",
        #! File Transfer Protocols (FTP, SFTP, etc.)
        FileTransfer = "File Transfer Protocols",
        #! Forms, Surveys & Scheduling
        FormsSurveys = "Forms, Surveys & Scheduling",
        #! Google Workspace Suite
        GoogleWorkspace = "Google Workspace Suite",
        #! Hospitality & Property Management
        Hospitality = "Hospitality & Property Management",
        #! HR & People Management
        Hr = "HR & People Management",
        #! IoT & Smart Building
        Iot = "IoT & Smart Building",
        #! Marketing Automation
        Marketing = "Marketing Automation",
        #! Messaging & Real-time Communication
        Messaging = "Messaging & Real-time Communication",
        #! Notifications & Alerts
        Notifications = "Notifications & Alerts",
        #! Payment Processing
        Payments = "Payment Processing",
        #! Project & Task Management
        ProjectManagement = "Project & Task Management",
        #! Social Media Management
        SocialMedia = "Social Media Management",
        #! Spreadsheets & Data Tables
        Spreadsheets = "Spreadsheets & Data Tables",
        #! Version Control & Code Repositories
        VersionControl = "Version Control & Code Repositories",
        #! Video Conferencing & Meetings
        VideoConferencing = "Video Conferencing & Meetings",
        #! Weather services
        Weather = "Weather",
        #! Shipping & Logistics
        Shipping = "Shipping & Logistics",
        #! Web Scraping & Automation
        WebAutomation = "Web Scraping & Automation",
    }
}
